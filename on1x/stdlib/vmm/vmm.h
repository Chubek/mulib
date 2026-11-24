#ifndef ON1X_STDLIB_VMM_H
#define ON1X_STDLIB_VMM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Trusted host API, not an On1x extern or a guest isolation boundary. */
enum {
    ON1X_VMM_DOMIX = 1,
    ON1X_VMM_DOMIOS = 2,
    ON1X_VMM_MAX_PATH_BYTES = 4096,
    ON1X_VMM_MAX_CMDLINE_BYTES = 2048,
    ON1X_VMM_MAX_INPUT_BYTES = 4096,
    ON1X_VMM_MAX_OUTPUT_BYTES = 4096,
    ON1X_VMM_ERROR_BYTES = 1024
};

typedef struct On1xVmmConfigV1 {
    size_t struct_size;
    const char *machine;   /* x86_64, aarch64, or riscv64 */
    const char *backend;   /* software, or auto with the mandatory instruction limit */
    const char *archml_path, *kernel_path, *image_path, *firmware_path, *cmdline;
    uint64_t memory_mb, instruction_limit;
    uint32_t flags;
} On1xVmmConfigV1;

typedef enum On1xVmmStatus {
    ON1X_VMM_OK = 0,
    ON1X_VMM_INVALID_CONFIG,
    ON1X_VMM_UNSUPPORTED_BACKEND,
    ON1X_VMM_BUSY,
    ON1X_VMM_CREATE_FAILED,
    ON1X_VMM_SERIAL_FAILED,
    ON1X_VMM_INITIALIZE_FAILED,
    ON1X_VMM_START_FAILED,
    ON1X_VMM_WAIT_FAILED
} On1xVmmStatus;

typedef struct On1xVmmReportV1 {
    size_t serial_input_used, serial_output_size;
    uint8_t serial_output[ON1X_VMM_MAX_OUTPUT_BYTES];
    int serial_output_truncated, error_truncated;
    char error[ON1X_VMM_ERROR_BYTES];
} On1xVmmReportV1;

/* config and its strings, input and report require valid C storage. Strings are
 * bounded NUL-terminated byte strings; optional strings may be NULL or empty.
 * Keep all argument storage valid and immutable (except report) until return.
 * report is required, written once the VM and callbacks have been destroyed.
 * Use independent report storage for each call. At most one VM operation is
 * active; overlaps return BUSY. Do not mix with raw VM calls.
 *
 * A positive software instruction budget (1..100,000,000) is required. That
 * budget is not a wall-clock deadline and does not bound boot-file I/O.
 * Input is a byte span, including NUL; reads after exhaustion return zero, as
 * required by the device byte API. The report contains the captured prefix
 * (not NUL terminated), plus an explicit truncation flag and copied diagnostic.
 * Only callers trusting the inputs, host environment and linked ArchVirt may
 * use this interface. No cancellation, detach, guest handle, or host terminal. */
On1xVmmStatus on1x_vmm_run_v1(const On1xVmmConfigV1 *config,
                            const uint8_t *input, size_t input_size,
                            On1xVmmReportV1 *report);

#ifdef __cplusplus
}
#endif
#endif
