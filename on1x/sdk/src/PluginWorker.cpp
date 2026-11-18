#include "PluginProtocol.hpp"
#include "ProgramInternal.hpp"
#include "RegistryInternal.hpp"
#include "mulib-ipc-framed.h"
#include "mulib-namespace.h"
#include "mulib-seccomp.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/personality.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <memory>
#include <signal.h>

namespace {
using namespace on1x::sdk_plugin;
constexpr int broker_fd = 3, image_fd = 4;

bool limit(int resource, rlim_t value)
{
    const rlimit bound{value, value};
    return setrlimit(resource, &bound) == 0;
}

bool stage_image()
{
    // The host passes one pinned file, not a pathname or a dependency directory.
    struct stat state{};
    if (fstat(image_fd, &state) || !S_ISREG(state.st_mode) ||
        state.st_size <= 0 || state.st_size > max_image)
        return false;
    const int out = open("/plugin.so", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0400);
    if (out < 0) return false;
    std::array<char, 16384> chunk{};
    std::size_t remaining = static_cast<std::size_t>(state.st_size);
    while (remaining) {
        const auto n = read(image_fd, chunk.data(), std::min(remaining, chunk.size()));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        for (ssize_t done = 0; done < n;) {
            const auto w = write(out, chunk.data() + done, static_cast<std::size_t>(n - done));
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) return false;
            done += w;
        }
        remaining -= static_cast<std::size_t>(n);
    }
    return close(out) == 0 && close(image_fd) == 0;
}

int install_filter()
{
    mulib_seccomp_policy_t policy{};
    if (const int result = mulib_seccomp_worker(&policy, broker_fd, 0); result)
        return result;

    // The entire filesystem is the private tmpfs with just /plugin.so.
    // No host filesystem (including /proc), directory fds, new mount,
    // socket, exec, ptrace, or child/thread creation is available.
    constexpr auto open_flags = static_cast<std::uint32_t>(O_CLOEXEC | O_LARGEFILE);
    const mulib_seccomp_check_t open_args[]{
        {0, UINT32_MAX, static_cast<std::uint32_t>(AT_FDCWD)},
        {2, UINT32_MAX & ~open_flags, 0}
    };
    if (const int result = mulib_seccomp_allow(&policy, SYS_openat, open_args, 2); result)
        return result;

    for (const int syscall : {SYS_read, SYS_pread64, SYS_fstat, SYS_newfstatat, SYS_lseek})
        if (const int result = mulib_seccomp_allow(&policy, syscall, nullptr, 0); result)
            return result;

    constexpr auto map_flags = static_cast<std::uint32_t>(
        MAP_PRIVATE | MAP_FIXED | MAP_DENYWRITE | MAP_ANONYMOUS);
    for (const auto protection : {PROT_NONE, PROT_READ, PROT_READ | PROT_WRITE, PROT_READ | PROT_EXEC}) {
        const auto forbidden = (UINT32_MAX & ~map_flags) |
                               (protection & PROT_EXEC ? MAP_ANONYMOUS : 0);
        const mulib_seccomp_check_t map_args[]{
            {2, UINT32_MAX, static_cast<std::uint32_t>(protection)},
            {3, forbidden | MAP_PRIVATE, MAP_PRIVATE}
        };
        if (const int result = mulib_seccomp_allow(&policy, SYS_mmap, map_args, 2); result)
            return result;
    }
    return mulib_seccomp_install(&policy);
}

bool enter(const char *mount_directory, pid_t parent)
{
    if (getppid() != parent || prctl(PR_SET_PDEATHSIG, SIGKILL) ||
        getuid() == 0 || getuid() != geteuid() || getgid() == 0 || getgid() != getegid())
        return false;
    const mulib_namespace_spec_t spec{
        MULIB_SB_NS_USER | MULIB_SB_NS_MOUNT | MULIB_SB_NS_NETWORK | MULIB_SB_NS_IPC, 0, 0
    };
    std::uint32_t applied = 0;
    if (mulib_namespace_enter(&spec, &applied) || applied != spec.flags ||
        mount("tmpfs", mount_directory, "tmpfs", MS_NOSUID | MS_NODEV, "mode=0700,size=20m") ||
        chdir(mount_directory) || chroot(".") || chdir("/") || !stage_image())
        return false;

    const int pers = personality(0xffffffffUL);
    __user_cap_header_struct cap{_LINUX_CAPABILITY_VERSION_3, 0};
    __user_cap_data_struct none[2]{};
    return pers >= 0 && !(pers & READ_IMPLIES_EXEC) &&
           prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0 &&
           prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) == 0 &&
           syscall(SYS_capset, &cap, none) == 0 &&
           prctl(PR_SET_DUMPABLE, 0) == 0 &&
           prctl(PR_SET_PDEATHSIG, SIGKILL) == 0 && getppid() == parent &&
           limit(RLIMIT_AS, 256 * MULIB_SANDBOX_MIB) && limit(RLIMIT_STACK, 8 * MULIB_SANDBOX_MIB) &&
           limit(RLIMIT_CPU, 3) && limit(RLIMIT_CORE, 0) && limit(RLIMIT_FSIZE, 0) &&
           limit(RLIMIT_NPROC, 0) && limit(RLIMIT_MEMLOCK, 0) && limit(RLIMIT_NOFILE, 8) &&
           install_filter() == 0;
}

struct Records {
    std::string bytes;
    std::uint32_t count = 0;
    bool failed = false;
};

extern "C" void collect(void *context, const On1xSDK_Diagnostic *record) noexcept
{
    auto &out = *static_cast<Records *>(context);
    if (out.failed) return;
    try {
        put(out.bytes, record->severity);
        put(out.bytes, record->byte_begin == SIZE_MAX ? UINT32_MAX : static_cast<std::uint32_t>(record->byte_begin));
        put(out.bytes, record->byte_end == SIZE_MAX ? UINT32_MAX : static_cast<std::uint32_t>(record->byte_end));
        put(out.bytes, record->code);
        put(out.bytes, record->message);
        ++out.count;
    } catch (...) {
        out.failed = true;
    }
}

std::string work(std::string_view request)
{
    Reader reader{request};
    if (reader.take() != version) throw std::runtime_error("unknown plugin request");
    const auto mode = reader.take();
    const std::string hook(reader.take(ON1X_SDK_NAME_MAX));
    const auto mir = reader.take(ON1X_SDK_MAX_PROGRAM_BYTES);
    reader.finish();
    if (mode > 1 || !on1x::sdk::valid_name(hook))
        throw std::runtime_error("invalid plugin request");

    const auto reply = [](On1xSDK_Status status) {
        std::string bytes;
        put(bytes, version);
        put(bytes, status);
        return bytes;
    };
    // dlopen may run constructors and IFUNC resolvers: this is intentionally
    // AFTER all isolation is installed, not merely before register_all.
    void *library = dlopen("/plugin.so", RTLD_NOW | RTLD_LOCAL);
    if (!library) return reply(ON1X_SDK_ERROR);
    using Entry = const On1xSDK_Extension *(*)();
    auto entry = reinterpret_cast<Entry>(dlsym(library, "on1x_sdk_linked_extension"));
    if (!entry) return reply(ON1X_SDK_ABI_MISMATCH);

    On1xSDK_Registry *raw_registry = nullptr;
    if (const auto status = on1x_sdk_registry_create(&raw_registry); status != ON1X_SDK_OK)
        return reply(status);
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    if (const auto status = on1x_sdk_registry_register(registry.get(), entry()); status != ON1X_SDK_OK)
        return reply(status == ON1X_SDK_UNSUPPORTED ? ON1X_SDK_ERROR : status);

    On1xSDK_Program *raw_program = nullptr;
    if (const auto status = on1x::sdk::make_verified_mir(mir, &raw_program); status != ON1X_SDK_OK)
        return reply(status);
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    if (mode == 0) {
        Records records;
        const On1xSDK_DiagnosticSink sink{collect, &records};
        const auto status = on1x_sdk_analyzer_run(registry.get(), hook.c_str(), program.get(), &sink);
        if (status != ON1X_SDK_OK) return reply(status);
        if (records.failed) return reply(ON1X_SDK_LIMIT);
        auto bytes = reply(ON1X_SDK_OK);
        put(bytes, records.count);
        bytes += records.bytes;
        return bytes;
    }
    On1xSDK_Artifact *raw_artifact = nullptr;
    const auto status = on1x_sdk_backend_run(registry.get(), hook.c_str(), program.get(),
                                            ON1X_SDK_MAX_ARTIFACT_BYTES, &raw_artifact);
    if (status != ON1X_SDK_OK) return reply(status);
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    if (on1x_sdk_artifact_bytes(artifact.get(), &data, &size) != ON1X_SDK_OK)
        return reply(ON1X_SDK_ERROR);
    auto bytes = reply(ON1X_SDK_OK);
    put(bytes, size ? std::string_view(reinterpret_cast<const char *>(data), size) : std::string_view{});
    return bytes;
}
}

int main(int argc, char **argv)
{
    const auto parent = getppid();
    if (argc != 2 || parent <= 1) return 125;
    mulib_ipc_framed_t ipc{};
    std::int64_t deadline = 0;
    // The launcher sends only a trusted staging-directory argument. No
    // plugin-supplied data is parsed until the sandbox is active.
    if (mulib_ipc_framed_init(&ipc, broker_fd, max_frame) ||
        mulib_ipc_deadline(timeout_ms, &deadline) || !enter(argv[1], parent) ||
        mulib_ipc_send(&ipc, ready.data(), ready.size(), deadline))
        return 125;
    try {
        std::string buffer(max_frame, '\0');
        std::size_t size = 0;
        if (mulib_ipc_receive(&ipc, buffer.data(), buffer.size(), &size, deadline)) _exit(125);
        const auto response = work(std::string_view(buffer.data(), size));
        // Do not run DSO destructors at process exit.
        _exit(mulib_ipc_send(&ipc, response.data(), response.size(), deadline) ? 125 : 0);
    } catch (...) {
        _exit(125);
    }
}
