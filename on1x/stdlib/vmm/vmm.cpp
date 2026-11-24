#include "vmm.h"
#include <archvirt.h>
#include <atomic>
#include <cstring>
#include <initializer_list>
#include <type_traits>

namespace {
static_assert(std::is_same_v<decltype(&archvirt_create), void *(*)()>);
static_assert(std::is_same_v<decltype(&archvirt_initialize), int (*)(void *, const ArchVirtConfig *)>);
static_assert(std::is_same_v<decltype(&archvirt_set_serial), int (*)(void *, ArchVirtSerial, void *)>);
static_assert(std::is_same_v<ArchVirtSerial, uint8_t (*)(void *, int, uint8_t)>);
static_assert(std::is_same_v<decltype(&archvirt_start), int (*)(void *)>);
static_assert(std::is_same_v<decltype(&archvirt_wait), int (*)(void *)>);
static_assert(std::is_same_v<decltype(&archvirt_stop), void (*)(void *)>);
static_assert(std::is_same_v<decltype(&archvirt_destroy), void (*)(void *)>);
static_assert(std::is_same_v<decltype(&archvirt_last_error), const char *(*)(void *)>);
static_assert(int(ARCHVIRT_DOMIX) == ON1X_VMM_DOMIX && int(ARCHVIRT_DOMIOS) == ON1X_VMM_DOMIOS);

std::atomic_flag busy = ATOMIC_FLAG_INIT;

struct Session {
    void *vm{};
    bool needs_stop{};

    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() {
        if (vm) {
            if (needs_stop) {
                try { archvirt_stop(vm); }
                catch (...) { /* Still attempt to join before destroying the callback owner. */ }
                try { (void)archvirt_wait(vm); }
                catch (...) { /* Pinned destroy also attempts to stop and join. */ }
            }
            archvirt_destroy(vm);
        }
        busy.clear(std::memory_order_release);
    }
};

struct Serial {
    const uint8_t *input;
    size_t input_size;
    On1xVmmReportV1& out;
};

extern "C" uint8_t serial_byte(void *user, int is_write, uint8_t value) noexcept {
    auto& state = *static_cast<Serial *>(user);
    auto& out = state.out;
    if (is_write) {
        if (out.serial_output_size < sizeof out.serial_output)
            out.serial_output[out.serial_output_size++] = value;
        else
            out.serial_output_truncated = 1;
        return 0;
    }
    return out.serial_input_used < state.input_size ? state.input[out.serial_input_used++] : 0;
}

bool empty(const char *s) { return !s || !*s; }

bool bounded(const char *s, size_t max) {
    if (!s) return true;
    for (size_t i = 0; i <= max; ++i)
        if (!s[i]) return true;
    return false;
}

On1xVmmStatus fail(On1xVmmReportV1& out, On1xVmmStatus status, const char *message) {
    size_t n = 0;
    while (n < sizeof out.error - 1 && message[n]) {
        out.error[n] = message[n];
        ++n;
    }
    out.error[n] = '\0';
    out.error_truncated = message[n] != '\0';
    return status;
}

On1xVmmStatus upstream_failure(On1xVmmReportV1& out, On1xVmmStatus status, void *vm,
                               const char *fallback) {
    // archvirt_last_error borrows Handle::error; copy before another call or destruction.
    const char *error = archvirt_last_error(vm);
    return fail(out, status, empty(error) ? fallback : error);
}

On1xVmmStatus run(const On1xVmmConfigV1 *config, const uint8_t *input, size_t input_size,
                  On1xVmmReportV1& out) {
    if (!config || config->struct_size != sizeof *config)
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Invalid On1xVmmConfigV1 size");
    const auto& in = *config;
    if (!in.machine || !bounded(in.machine, 7) ||
        (std::strcmp(in.machine, "x86_64") && std::strcmp(in.machine, "aarch64") &&
         std::strcmp(in.machine, "riscv64")))
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Expected x86_64, aarch64, or riscv64");
    if (!in.backend || !bounded(in.backend, 8) ||
        (std::strcmp(in.backend, "software") && std::strcmp(in.backend, "auto")))
        return fail(out, ON1X_VMM_UNSUPPORTED_BACKEND, "This bridge requires software or bounded auto");
    if (in.memory_mb < 40 || in.memory_mb > 1024)
        return fail(out, ON1X_VMM_INVALID_CONFIG, "RAM must be 40..1024 MiB");
    if (!in.instruction_limit || in.instruction_limit > 100000000)
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Instruction limit must be 1..100,000,000");
    if (in.flags & ~uint32_t(ON1X_VMM_DOMIX | ON1X_VMM_DOMIOS))
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Unknown boot flags");
    for (const char *s : {in.archml_path, in.kernel_path, in.image_path, in.firmware_path})
        if (!bounded(s, ON1X_VMM_MAX_PATH_BYTES))
            return fail(out, ON1X_VMM_INVALID_CONFIG, "Path exceeds 4096 bytes");
    if (!bounded(in.cmdline, ON1X_VMM_MAX_CMDLINE_BYTES))
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Command line exceeds 2048 bytes");
    if (empty(in.kernel_path) && !(in.flags & ON1X_VMM_DOMIX))
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Kernel path or DOMIX required");
    if (!empty(in.firmware_path) && !(in.flags & ON1X_VMM_DOMIOS))
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Firmware path requires DOMIOS");
    if (input_size > ON1X_VMM_MAX_INPUT_BYTES || (input_size && !input))
        return fail(out, ON1X_VMM_INVALID_CONFIG, "Invalid serial input span");
    if (busy.test_and_set(std::memory_order_acquire))
        return fail(out, ON1X_VMM_BUSY, "Another VM operation is active");

    // Destruction order matters: Session stops and joins before Serial dies.
    // Pinned software backends invoke serial on the one VM worker; no user
    // callback can re-enter this host API, and the host reads out after join.
    Serial serial{input, input_size, out};
    Session session;
    ArchVirtConfig native{};
    native.struct_size = sizeof native;
    native.machine = in.machine;
    native.backend = in.backend;
    native.archml_path = in.archml_path;
    native.kernel_path = in.kernel_path;
    native.image_path = in.image_path;
    native.firmware_path = in.firmware_path;
    native.cmdline = in.cmdline;
    native.memory_mb = in.memory_mb;
    native.instruction_limit = in.instruction_limit;
    native.flags = in.flags;
    auto stage = ON1X_VMM_CREATE_FAILED;
    try {
        session.vm = archvirt_create();
        if (!session.vm) return fail(out, stage, "ArchVirt could not allocate a VM");
        stage = ON1X_VMM_SERIAL_FAILED;
        if (archvirt_set_serial(session.vm, serial_byte, &serial))
            return upstream_failure(out, stage, session.vm, "ArchVirt rejected serial registration");
        stage = ON1X_VMM_INITIALIZE_FAILED;
        if (archvirt_initialize(session.vm, &native))
            return upstream_failure(out, stage, session.vm, "ArchVirt could not initialize the VM");
        stage = ON1X_VMM_START_FAILED;
        session.needs_stop = true;
        if (archvirt_start(session.vm))
            return upstream_failure(out, stage, session.vm, "ArchVirt could not start the VM");
        stage = ON1X_VMM_WAIT_FAILED;
        // wait joins even when the guest fails (e.g. instruction-limit exit).
        // Still request stop on any nonzero result: the C API also maps join
        // failure to the same status.
        if (archvirt_wait(session.vm))
            return upstream_failure(out, stage, session.vm, "ArchVirt VM execution failed");
        session.needs_stop = false;
        return ON1X_VMM_OK;
    } catch (...) {
        return fail(out, stage, "Unexpected ArchVirt exception");
    }
}
} // namespace

extern "C" On1xVmmStatus on1x_vmm_run_v1(const On1xVmmConfigV1 *config,
                                       const uint8_t *input, size_t input_size,
                                       On1xVmmReportV1 *report) {
    if (!report) return ON1X_VMM_INVALID_CONFIG;
    On1xVmmReportV1 out{};
    const auto status = run(config, input, input_size, out);
    *report = out;
    return status;
}
