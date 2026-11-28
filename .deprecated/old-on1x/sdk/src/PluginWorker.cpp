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
#include <cstring>
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

struct WorkResult {
    On1xSDK_Status status = ON1X_SDK_ERROR;
    std::size_t output_size = 0;
};

WorkResult work(const Setup &setup, std::string_view mir,
                std::uint8_t *output, std::size_t output_capacity)
{
    // dlopen may run constructors and IFUNC resolvers: this is intentionally
    // AFTER all isolation is installed, not merely before register_all.
    void *library = dlopen("/plugin.so", RTLD_NOW | RTLD_LOCAL);
    if (!library) return {ON1X_SDK_ERROR, 0};
    using Entry = const On1xSDK_Extension *(*)();
    auto entry = reinterpret_cast<Entry>(dlsym(library, "on1x_sdk_linked_extension"));
    if (!entry) return {ON1X_SDK_ABI_MISMATCH, 0};

    On1xSDK_Registry *raw_registry = nullptr;
    if (const auto status = on1x_sdk_registry_create(&raw_registry); status != ON1X_SDK_OK)
        return {status, 0};
    const std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(raw_registry, on1x_sdk_registry_destroy);
    if (const auto status = on1x_sdk_registry_register(registry.get(), entry()); status != ON1X_SDK_OK)
        return {status == ON1X_SDK_UNSUPPORTED ? ON1X_SDK_ERROR : status, 0};

    On1xSDK_Program *raw_program = nullptr;
    if (const auto status = on1x::sdk::make_verified_mir(mir, &raw_program); status != ON1X_SDK_OK)
        return {status, 0};
    const std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>
        program(raw_program, on1x_sdk_program_destroy);
    if (setup.mode == 0) {
        Records records;
        const On1xSDK_DiagnosticSink sink{collect, &records};
        const auto status = on1x_sdk_analyzer_run(
            registry.get(), setup.hook.c_str(), program.get(), &sink
        );
        if (status != ON1X_SDK_OK) return {status, 0};
        if (records.failed) return {ON1X_SDK_LIMIT, 0};
        std::string bytes;
        put(bytes, records.count);
        bytes += records.bytes;
        if (bytes.size() > output_capacity)
            return {ON1X_SDK_LIMIT, 0};
        if (!bytes.empty()) std::memcpy(output, bytes.data(), bytes.size());
        return {ON1X_SDK_OK, bytes.size()};
    }
    On1xSDK_Artifact *raw_artifact = nullptr;
    const auto status = on1x_sdk_backend_run(registry.get(), setup.hook.c_str(), program.get(),
                                            ON1X_SDK_MAX_ARTIFACT_BYTES, &raw_artifact);
    if (status != ON1X_SDK_OK) return {status, 0};
    const std::unique_ptr<On1xSDK_Artifact, decltype(&on1x_sdk_artifact_destroy)>
        artifact(raw_artifact, on1x_sdk_artifact_destroy);
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    if (on1x_sdk_artifact_bytes(artifact.get(), &data, &size) != ON1X_SDK_OK)
        return {ON1X_SDK_ERROR, 0};
    if (size > output_capacity) return {ON1X_SDK_LIMIT, 0};
    if (size) std::memcpy(output, data, size);
    return {ON1X_SDK_OK, size};
}
}

int main(int argc, char **argv)
{
    const auto parent = getppid();
    if (argc != 2 || parent <= 1) return 125;
    mulib_ipc_framed_t ipc{};
    std::int64_t deadline = 0;
    if (mulib_ipc_framed_init(&ipc, broker_fd, max_frame) ||
        mulib_ipc_deadline(timeout_ms, &deadline))
        return 125;

    // This setup frame is produced by the trusted host. It contains bounded
    // declarative metadata and generation-checked shared-memory descriptors,
    // never plugin code or a host pointer. Attach before CLONE_NEWIPC, then
    // establish every isolation layer before dlopen can run constructors.
    std::string control(max_frame, '\0');
    std::size_t control_size = 0;
    Setup setup;
    On1xSDK_SharedMemory *raw_input = nullptr, *raw_output = nullptr;
    try {
        if (mulib_ipc_receive(
                &ipc, control.data(), control.size(), &control_size,
                deadline))
            return 125;
        const auto hello = decode_message(
            {control.data(), control_size}
        );
        if (hello.kind != ON1X_SDK_PROTOCOL_HELLO ||
            hello.status != ON1X_SDK_OK || hello.request_id ||
            hello.owner_context || hello.object_id || hello.revision)
            return 125;
        setup = decode_setup({
            reinterpret_cast<const char *>(hello.payload.data),
            hello.payload.size
        });
        if (setup.input.size > ON1X_SDK_MAX_PROGRAM_BYTES ||
            setup.output.capacity < ON1X_SDK_MAX_ARTIFACT_BYTES ||
            setup.output.capacity > ON1X_SDK_SHARED_MEMORY_MAX_BYTES ||
            setup.input.owner_context != setup.output.owner_context ||
            setup.input.handle == setup.output.handle ||
            setup.input.platform_id == setup.output.platform_id)
            return 125;
        if (on1x_sdk_shared_memory_import_authorized(
                &setup.input, ON1X_SDK_SHARED_MEMORY_FROZEN,
                &raw_input) != ON1X_SDK_OK ||
            on1x_sdk_shared_memory_import_authorized(
                &setup.output, ON1X_SDK_SHARED_MEMORY_STAGING,
                &raw_output) != ON1X_SDK_OK)
            return 125;
    } catch (...) {
        return 125;
    }
    const std::unique_ptr<On1xSDK_SharedMemory, decltype(&on1x_sdk_shared_memory_release)>
        input(raw_input, on1x_sdk_shared_memory_release);
    const std::unique_ptr<On1xSDK_SharedMemory, decltype(&on1x_sdk_shared_memory_release)>
        output(raw_output, on1x_sdk_shared_memory_release);

    if (!enter(argv[1], parent)) return 125;
    try {
        const auto acknowledged =
            encode_message(ON1X_SDK_PROTOCOL_HELLO_ACK);
        if (mulib_ipc_send(
                &ipc, acknowledged.data(), acknowledged.size(),
                deadline))
            _exit(125);
        if (mulib_ipc_receive(
                &ipc, control.data(), control.size(), &control_size,
                deadline))
            _exit(125);
        const auto activate = decode_message(
            {control.data(), control_size}
        );
        if (activate.kind != ON1X_SDK_PROTOCOL_ACTIVATE ||
            activate.status != ON1X_SDK_OK ||
            activate.request_id || activate.owner_context ||
            activate.object_id || activate.revision ||
            activate.payload.size)
            _exit(125);
        const auto active = encode_message(ON1X_SDK_PROTOCOL_ACTIVE);
        if (mulib_ipc_send(
                &ipc, active.data(), active.size(), deadline))
            _exit(125);
        if (mulib_ipc_receive(
                &ipc, control.data(), control.size(), &control_size,
                deadline))
            _exit(125);
        const auto operation = decode_message(
            {control.data(), control_size}
        );
        if (operation.kind != ON1X_SDK_PROTOCOL_REQUEST ||
            operation.status != ON1X_SDK_OK ||
            operation.request_id != request_id ||
            operation.owner_context || operation.object_id ||
            operation.revision ||
            operation.flags || operation.payload.size)
            _exit(125);

        const std::uint8_t *input_bytes = nullptr;
        std::size_t input_size = 0;
        std::uint8_t *output_bytes = nullptr;
        std::size_t output_capacity = 0;
        if (on1x_sdk_shared_memory_bytes(
                input.get(), &input_bytes, &input_size) != ON1X_SDK_OK ||
            on1x_sdk_shared_memory_mutable_bytes(
                output.get(), &output_bytes, &output_capacity) !=
                ON1X_SDK_OK)
            _exit(125);
        const auto result = work(
            setup,
            {reinterpret_cast<const char *>(input_bytes), input_size},
            output_bytes, output_capacity
        );
        std::string response;
        if (result.status == ON1X_SDK_OK) {
            On1xSDK_SharedMemoryDescriptor descriptor{};
            if (on1x_sdk_shared_memory_freeze(
                    output.get(), result.output_size) != ON1X_SDK_OK ||
                on1x_sdk_shared_memory_export(
                    output.get(), &descriptor) != ON1X_SDK_OK)
                _exit(125);
            std::string payload(
                ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE, '\0'
            );
            if (on1x_sdk_shared_memory_descriptor_encode(
                    &descriptor, payload.data(), payload.size()) !=
                ON1X_SDK_OK)
                _exit(125);
            response = encode_message(
                ON1X_SDK_PROTOCOL_RESPONSE, ON1X_SDK_OK,
                request_id, payload,
                ON1X_SDK_PROTOCOL_FLAG_SHARED_PAYLOAD |
                    ON1X_SDK_PROTOCOL_FLAG_FINAL
            );
        } else {
            response = encode_message(
                ON1X_SDK_PROTOCOL_RESPONSE, result.status,
                request_id, {},
                ON1X_SDK_PROTOCOL_FLAG_FINAL
            );
        }
        // Do not run DSO destructors at process exit.
        _exit(mulib_ipc_send(&ipc, response.data(), response.size(), deadline) ? 125 : 0);
    } catch (...) {
        _exit(125);
    }
}
