#include "PluginHost.hpp"
#include "PluginProtocol.hpp"
#include "RegistryInternal.hpp"
#include "mulib-ipc-framed.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <set>
#include <signal.h>
#include <sys/auxv.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace on1x::sdk_plugin {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t max_paths = 32, max_entries = 4096, max_path_bytes = 4096;

struct Fd {
    int value;
    explicit Fd(int fd) : value(fd) {}
    Fd(const Fd &) = delete;
    ~Fd() { if (value >= 0) close(value); }
};

bool has_so_suffix(const fs::path &path) { return path.extension() == ".so"; }

bool matches_name(const fs::path &path, std::string_view name)
{
    const auto stem = path.stem().string();
    return path.filename().string() == name || stem == name ||
           (stem.starts_with("lib") && std::string_view(stem).substr(3) == name);
}

std::string normalized_path(const fs::path &path)
{
    // Resolve only the directory. Never follow a final symlink to a plugin.
    const auto result = (fs::canonical(path.has_parent_path() ? path.parent_path() : ".") /
                         path.filename()).string();
    if (result.size() > max_path_bytes)
        throw std::runtime_error("plugin path byte limit exceeded");
    return result;
}

std::vector<std::string> scan_paths(const std::vector<std::string> &selectors)
{
    // secure_getenv refuses use of the environment in a privileged invocation.
    const char *value = secure_getenv("ON1X_PLUGIN_PATH");
    if (!value || !value[0]) return {};
    if (strnlen(value, max_path_bytes + 1) > max_path_bytes)
        throw std::runtime_error("ON1X_PLUGIN_PATH byte limit exceeded");
    std::string_view parts(value);
    std::set<std::string> seen_dirs;
    std::size_t dirs = 0, entries = 0;
    std::vector<std::string> matches;
    for (;;) {
        const auto colon = parts.find(':');
        const auto component = parts.substr(0, colon);
        if (!component.empty()) {
            if (++dirs > max_paths) throw std::runtime_error("plugin directory limit exceeded");
            std::error_code error;
            const auto requested = fs::path(component);
            const auto path = fs::canonical(requested, error);
            if (error && error != std::errc::no_such_file_or_directory && error != std::errc::not_a_directory)
                throw fs::filesystem_error("cannot resolve plugin directory", requested, error);
            if (!error && seen_dirs.insert(path.string()).second) {
                std::vector<std::string> sorted;
                for (const auto &entry : fs::directory_iterator(path)) {
                    if (++entries > max_entries) throw std::runtime_error("plugin directory entry limit exceeded");
                    const auto &file = entry.path();
                    if (!has_so_suffix(file) || !fs::is_regular_file(entry.symlink_status())) continue;
                    if (!selectors.empty() && std::none_of(selectors.begin(), selectors.end(),
                        [&](const std::string &s) { return matches_name(file, s); }))
                        continue;
                    if (sorted.size() + matches.size() >= max_paths)
                        throw std::runtime_error("plugin candidate limit exceeded");
                    const auto candidate = file.string();
                    if (candidate.size() > max_path_bytes)
                        throw std::runtime_error("plugin path byte limit exceeded");
                    sorted.push_back(candidate);
                }
                std::sort(sorted.begin(), sorted.end());
                matches.insert(matches.end(), sorted.begin(), sorted.end());
            }
        }
        if (colon == std::string_view::npos) return matches;
        parts.remove_prefix(colon + 1);
    }
}

int open_image(const std::string &path)
{
    const Fd pinned(open(path.c_str(), O_PATH | O_CLOEXEC | O_NOFOLLOW));
    struct stat state{};
    if (pinned.value < 0 || fstat(pinned.value, &state) || !S_ISREG(state.st_mode) ||
        state.st_size <= 0 || state.st_size > max_image)
        throw std::runtime_error("plugin is not a readable regular .so within the size limit");
    const auto name = "/proc/self/fd/" + std::to_string(pinned.value);
    const int fd = open(name.c_str(), O_RDONLY | O_CLOEXEC | O_NOCTTY);
    if (fd < 0) throw std::runtime_error("cannot open plugin image");
    return fd;
}

int open_worker()
{
    const auto compiler = fs::read_symlink("/proc/self/exe");
    const auto path = compiler.parent_path() / "on1x-plugin-worker";
    const Fd pinned(open(path.c_str(), O_PATH | O_CLOEXEC | O_NOFOLLOW));
    struct stat host{}, worker{};
    if (pinned.value < 0 || stat("/proc/self/exe", &host) || fstat(pinned.value, &worker) ||
        !S_ISREG(worker.st_mode) || host.st_uid != worker.st_uid ||
        (worker.st_mode & (S_ISUID | S_ISGID | S_IWGRP | S_IWOTH)))
        throw std::runtime_error("trusted on1x-plugin-worker is missing or has unsafe ownership or permissions");
    const auto name = "/proc/self/fd/" + std::to_string(pinned.value);
    const int fd = open(name.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) throw std::runtime_error("cannot pin on1x-plugin-worker");
    unsigned char magic[4]{};
    if (pread(fd, magic, sizeof magic, 0) != sizeof magic ||
        std::memcmp(magic, "\177ELF", sizeof magic) != 0) {
        close(fd);
        throw std::runtime_error("on1x-plugin-worker is not an ELF executable");
    }
    return fd;
}

struct Stage {
    char path[sizeof("/tmp/on1x-plugin-XXXXXX")] = "/tmp/on1x-plugin-XXXXXX";
    Stage() { if (!mkdtemp(path)) throw std::runtime_error("cannot create plugin isolation mount point"); }
    ~Stage() { (void)rmdir(path); }
};

struct Worker {
    pid_t pid;
    ~Worker() {
        if (pid > 0) {
            (void)kill(pid, SIGKILL);
            std::int64_t deadline = 0;
            if (mulib_ipc_deadline(1000, &deadline) == 0)
                (void)finish(deadline);
            // A child stuck in an uninterruptible kernel wait must not keep
            // the driver blocked. On this failure path the driver exits.
        }
    }
    bool finish(std::int64_t deadline) {
        for (;;) {
            const auto now = mulib_ipc_now_ms();
            if (now < 0 || now >= deadline)
                return false;
            int status = 0;
            const auto result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                pid = -1;
                return WIFEXITED(status) && WEXITSTATUS(status) == 0;
            }
            if (result < 0 && errno != EINTR) {
                if (errno == ECHILD) pid = -1;
                return false;
            }
            const timespec sleep{0, 1000000};
            nanosleep(&sleep, nullptr);
        }
        return false;
    }
};

[[noreturn]] void exec_worker(int worker, int broker, int image, char *stage)
{
    if (dup3(broker, 3, 0) < 0 || dup3(image, 4, 0) < 0 || dup3(worker, 5, O_CLOEXEC) < 0 ||
        syscall(SYS_close_range, 6u, ~0u, 0u))
        _exit(125);
    for (int fd = 0; fd < 3; ++fd) (void)close(fd);
    char name[] = "on1x-plugin-worker", locale[] = "LC_ALL=C", bind[] = "LD_BIND_NOW=1";
    char *const argv[]{name, stage, nullptr};
    char *const env[]{locale, bind, nullptr};
    syscall(SYS_execveat, 5, "", argv, env, AT_EMPTY_PATH);
    _exit(125);
}

Reply parse_reply(On1xSDK_Status status, std::string_view bytes, bool emit,
                  std::size_t mir_size)
{
    if (status > ON1X_SDK_BUSY)
        throw std::runtime_error("invalid plugin reply status");
    Reply reply{status, {}, {}};
    if (status != ON1X_SDK_OK) {
        if (!bytes.empty())
            throw std::runtime_error("failed plugin reply carried output");
        return reply;
    }
    if (emit) {
        if (bytes.size() > ON1X_SDK_MAX_ARTIFACT_BYTES)
            throw std::runtime_error("plugin artifact size exceeded");
        reply.artifact.assign(bytes);
        return reply;
    }
    Reader reader{bytes};
    const auto count = reader.take();
    if (count > ON1X_SDK_MAX_ANALYZER_DIAGNOSTICS)
        throw std::runtime_error("plugin diagnostic count exceeded");
    for (unsigned i = 0; i < count; ++i) {
        const auto severity = reader.take(), begin = reader.take(), end = reader.take();
        const auto code = reader.take(ON1X_SDK_NAME_MAX);
        const auto message = reader.take(ON1X_SDK_MAX_DIAGNOSTIC_MESSAGE_BYTES);
        if (severity < ON1X_SDK_NOTE || severity > ON1X_SDK_DIAGNOSTIC_ERROR ||
            !sdk::valid_name(code) || message.empty() ||
            message.find('\0') != std::string_view::npos ||
            (!(begin == UINT32_MAX && end == UINT32_MAX) && (begin > end || end > mir_size))) {
            throw std::runtime_error("invalid plugin diagnostic");
        }
        reply.diagnostics.push_back({severity, std::string(code), std::string(message),
                                      begin == UINT32_MAX ? SIZE_MAX : begin,
                                      end == UINT32_MAX ? SIZE_MAX : end});
    }
    reader.finish();
    return reply;
}

Reply call(int worker, const std::string &path, std::string_view hook, bool emit,
           std::string_view mir, std::int64_t deadline)
{
    const Fd image(open_image(path));
    Stage stage;
    On1xSDK_Context *raw_context = nullptr;
    if (on1x_sdk_context_create(nullptr, &raw_context) != ON1X_SDK_OK)
        throw std::runtime_error("cannot create plugin data-plane context");
    const std::unique_ptr<On1xSDK_Context, decltype(&on1x_sdk_context_release)>
        context(raw_context, on1x_sdk_context_release);
    On1xSDK_SharedMemory *raw_input = nullptr, *raw_output = nullptr;
    if (on1x_sdk_shared_memory_create_frozen(
            context.get(),
            {reinterpret_cast<const std::uint8_t *>(mir.data()), mir.size()},
            &raw_input) != ON1X_SDK_OK)
        throw std::runtime_error("cannot create immutable plugin input");
    const std::unique_ptr<On1xSDK_SharedMemory, decltype(&on1x_sdk_shared_memory_release)>
        input(raw_input, on1x_sdk_shared_memory_release);
    if (on1x_sdk_shared_memory_create_staging(
            context.get(), ON1X_SDK_MAX_ARTIFACT_BYTES, &raw_output) !=
        ON1X_SDK_OK)
        throw std::runtime_error("cannot create plugin output staging region");
    const std::unique_ptr<On1xSDK_SharedMemory, decltype(&on1x_sdk_shared_memory_release)>
        output(raw_output, on1x_sdk_shared_memory_release);
    On1xSDK_SharedMemoryDescriptor input_descriptor{}, output_descriptor{};
    if (on1x_sdk_shared_memory_export(input.get(), &input_descriptor) != ON1X_SDK_OK ||
        on1x_sdk_shared_memory_export_staging(output.get(), &output_descriptor) !=
            ON1X_SDK_OK)
        throw std::runtime_error("cannot export plugin data-plane descriptors");
    const auto setup = encode_setup(
        emit ? 1u : 0u, hook, input_descriptor, output_descriptor
    );
    const auto hello = encode_message(
        ON1X_SDK_PROTOCOL_HELLO, ON1X_SDK_OK, 0, setup
    );

    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair))
        throw std::runtime_error("cannot create plugin control socket");
    const Fd parent(pair[0]);
    mulib_ipc_framed_t ipc{};
    if (mulib_ipc_framed_init(&ipc, parent.value, max_frame)) {
        close(pair[1]);
        throw std::runtime_error("cannot initialize plugin control socket");
    }
    pid_t pid;
    {
        const Fd peer(pair[1]), worker_high(fcntl(worker, F_DUPFD_CLOEXEC, 10)),
            peer_high(fcntl(peer.value, F_DUPFD_CLOEXEC, 10)),
            image_high(fcntl(image.value, F_DUPFD_CLOEXEC, 10));
        if (worker_high.value < 0 || peer_high.value < 0 || image_high.value < 0)
            throw std::runtime_error("cannot pin plugin worker handles");
        pid = fork();
        if (pid < 0) throw std::runtime_error("cannot start plugin worker");
        if (!pid) exec_worker(worker_high.value, peer_high.value, image_high.value, stage.path);
    }
    Worker child{pid};
    std::string buffer(max_frame, '\0');
    std::size_t size = 0;
    if (const int error = mulib_ipc_send(
            &ipc, hello.data(), hello.size(), deadline); error)
        throw std::runtime_error(std::string("cannot send plugin setup: ") +
                                 mulib_sandbox_strerror(error));
    if (const int error = mulib_ipc_receive(
            &ipc, buffer.data(), buffer.size(), &size, deadline); error)
        throw std::runtime_error(error == MULIB_SB_TIMEOUT ? "plugin isolation startup timed out" :
                                  "plugin isolation unavailable; worker did not enter the sandbox");
    auto message = decode_message({buffer.data(), size});
    if (message.kind != ON1X_SDK_PROTOCOL_HELLO_ACK ||
        message.status != ON1X_SDK_OK || message.request_id ||
        message.owner_context || message.object_id || message.revision ||
        message.payload.size)
        throw std::runtime_error("plugin isolation handshake was invalid");

    const auto activate = encode_message(ON1X_SDK_PROTOCOL_ACTIVATE);
    if (const int error = mulib_ipc_send(
            &ipc, activate.data(), activate.size(), deadline); error)
        throw std::runtime_error(std::string("cannot activate plugin: ") +
                                 mulib_sandbox_strerror(error));
    if (const int error = mulib_ipc_receive(
            &ipc, buffer.data(), buffer.size(), &size, deadline); error)
        throw std::runtime_error(error == MULIB_SB_TIMEOUT ?
            "plugin activation timed out" :
            "plugin worker failed or was terminated by its sandbox");
    message = decode_message({buffer.data(), size});
    if (message.kind != ON1X_SDK_PROTOCOL_ACTIVE ||
        message.status != ON1X_SDK_OK || message.request_id ||
        message.owner_context || message.object_id || message.revision ||
        message.payload.size)
        throw std::runtime_error("plugin activation handshake was invalid");

    const auto operation = encode_message(
        ON1X_SDK_PROTOCOL_REQUEST, ON1X_SDK_OK, request_id
    );
    if (const int error = mulib_ipc_send(
            &ipc, operation.data(), operation.size(), deadline); error)
        throw std::runtime_error(std::string("cannot send plugin operation: ") +
                                 mulib_sandbox_strerror(error));
    if (const int error = mulib_ipc_receive(
            &ipc, buffer.data(), buffer.size(), &size, deadline); error)
        throw std::runtime_error(error == MULIB_SB_TIMEOUT ? "plugin execution timed out" :
                                  "plugin worker failed or was terminated by its sandbox");
    if (!child.finish(deadline)) throw std::runtime_error("plugin worker did not exit successfully");
    message = decode_message({buffer.data(), size});
    if (message.kind != ON1X_SDK_PROTOCOL_RESPONSE ||
        message.request_id != request_id ||
        message.owner_context || message.object_id || message.revision ||
        (message.flags & ON1X_SDK_PROTOCOL_FLAG_FINAL) == 0)
        throw std::runtime_error("plugin response state was invalid");
    if (message.status != ON1X_SDK_OK) {
        if (message.payload.size ||
            (message.flags & ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD))
            throw std::runtime_error("failed plugin response carried output");
        return parse_reply(message.status, {}, emit, mir.size());
    }
    if (message.flags !=
            (ON1X_SDK_PROTOCOL_FLAG_FINAL |
             ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD) ||
        message.payload.size != ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE)
        throw std::runtime_error("successful plugin response lacked data-plane output");
    On1xSDK_SharedMemoryDescriptor worker_output{};
    if (on1x_sdk_shared_memory_descriptor_decode(
            message.payload.data, message.payload.size, &worker_output) !=
            ON1X_SDK_OK ||
        worker_output.owner_context != output_descriptor.owner_context ||
        worker_output.handle != output_descriptor.handle ||
        worker_output.platform_id != output_descriptor.platform_id ||
        worker_output.capacity != output_descriptor.capacity ||
        worker_output.size > output_descriptor.capacity ||
        worker_output.flags != ON1X_SDK_SHARED_MEMORY_FROZEN ||
        worker_output.state != ON1X_SDK_HANDLE_STATE_FROZEN)
        throw std::runtime_error("plugin returned an unauthorized data-plane handle");
    if (on1x_sdk_shared_memory_freeze(output.get(), worker_output.size) !=
        ON1X_SDK_OK)
        throw std::runtime_error("cannot freeze plugin output");
    On1xSDK_SharedMemoryDescriptor local_output{};
    if (on1x_sdk_shared_memory_export(output.get(), &local_output) !=
            ON1X_SDK_OK ||
        !same_descriptor(local_output, worker_output) ||
        on1x_sdk_shared_memory_validate_local(
            context.get(), &local_output) != ON1X_SDK_OK)
        throw std::runtime_error("plugin output digest or handle verification failed");
    const std::uint8_t *output_bytes = nullptr;
    std::size_t output_size = 0;
    if (on1x_sdk_shared_memory_bytes(
            output.get(), &output_bytes, &output_size) != ON1X_SDK_OK)
        throw std::runtime_error("cannot read frozen plugin output");
    return parse_reply(
        message.status,
        {reinterpret_cast<const char *>(output_bytes), output_size},
        emit, mir.size()
    );
}
}

std::vector<std::string> discover(const std::vector<std::string> &selectors)
{
    if (selectors.size() > max_paths)
        throw std::runtime_error("plugin selector limit exceeded");
    std::vector<std::string> names;
    for (const auto &s : selectors) {
        if (s.empty() || s.size() > max_path_bytes)
            throw std::runtime_error("invalid plugin selector length");
        if (s.find('/') == std::string::npos) names.push_back(s);
    }
    if (selectors.empty()) return scan_paths({});
    const auto candidates = names.empty() ? std::vector<std::string>{} : scan_paths(names);
    std::vector<std::string> result;
    for (const auto &s : selectors) {
        std::string selected;
        if (s.find('/') == std::string::npos) {
            for (const auto &file : candidates) {
                if (!matches_name(file, s)) continue;
                if (!selected.empty()) throw std::runtime_error("plugin name is ambiguous: " + s);
                selected = file;
            }
            if (selected.empty()) throw std::runtime_error("plugin not found in ON1X_PLUGIN_PATH: " + s);
        } else {
            if (!has_so_suffix(s)) throw std::runtime_error("plugin path must end in .so: " + s);
            selected = normalized_path(s);
            if (!fs::is_regular_file(fs::symlink_status(selected)))
                throw std::runtime_error("plugin must be a regular, non-symlink file: " + selected);
        }
        if (std::find(result.begin(), result.end(), selected) == result.end())
            result.push_back(std::move(selected));
    }
    return result;
}

std::optional<Reply> dispatch(const std::vector<std::string> &paths, std::string_view hook,
                              bool emit, std::string_view mir)
{
    if (paths.empty()) return {};
    if (getauxval(AT_SECURE) || getuid() == 0 || getuid() != geteuid() ||
        getgid() == 0 || getgid() != getegid())
        throw std::runtime_error("external plugins require an unprivileged On1x driver");
    struct sigaction sa{};
    if (sigaction(SIGCHLD, nullptr, &sa) || sa.sa_handler != SIG_DFL || (sa.sa_flags & SA_NOCLDWAIT))
        throw std::runtime_error("cannot supervise plugins with nondefault SIGCHLD handling");
    const Fd worker(open_worker());
    std::int64_t deadline = 0;
    if (mulib_ipc_deadline(timeout_ms, &deadline)) throw std::runtime_error("cannot set plugin deadline");
    std::optional<Reply> result;
    for (const auto &path : paths) {
        Reply reply;
        try {
            reply = call(worker.value, path, hook, emit, mir, deadline);
        } catch (const std::exception &error) {
            throw std::runtime_error(path + ": " + error.what());
        }
        if (reply.status == ON1X_SDK_UNSUPPORTED) continue;
        if (reply.status != ON1X_SDK_OK) return reply;
        if (result) return Reply{ON1X_SDK_DUPLICATE, {}, {}};
        result = std::move(reply);
    }
    return result;
}
}
