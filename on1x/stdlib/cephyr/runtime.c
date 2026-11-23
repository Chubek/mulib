#include <cephyr/Cephyr.h>
#include "on1x_runtime.h"
#include <string.h>

/* Immutable On1x text snapshots outlive replacement of these slots. */
static _Thread_local const char *last_output = "";
static _Thread_local const char *last_diagnostics = "";
enum { text_limit = 4 * 1024 * 1024 };

const char *on1x_cephyr_output_v1(void) { return last_output; }
const char *on1x_cephyr_diagnostics_v1(void) { return last_diagnostics; }

static int valid_text(const char *data, size_t n) {
    return n == 0 || (data && n <= text_limit && !memchr(data, 0, n));
}

int64_t on1x_cephyr_analyze_v1(const char *source, int64_t mode) {
    last_output = "";
    last_diagnostics = "";
    if (!source || mode < 0 || mode > 2) {
        last_diagnostics = "invalid Cephyr analysis request";
        return CEPHYR_INVALID_ARGUMENT;
    }
    size_t size = 0;
    while (size <= text_limit && source[size]) ++size;
    if (size > text_limit) {
        last_diagnostics = "Cephyr source exceeds 4 MiB";
        return CEPHYR_INVALID_ARGUMENT;
    }
    if (cephyr_abi_version() != 1) {
        last_diagnostics = "expected Cephyr embedding ABI 1";
        return CEPHYR_INVALID_ARGUMENT;
    }
    /* Explicit baseline: ignore caller's ./Cephyr.toml and CEPHYR_CONFIG. */
    const char *args[] = {"--config", ON1X_CEPHYR_CONFIG, "--target=x86_64-linux-gnu",
                          "--no-sandslab", "--no-factir", "-x", "c",
                          mode == 0 ? "-fsyntax-only" : mode == 1 ? "--emit-mir" : "-S",
                          "-", "-o", "-"};
    cephyr_request request = {
        .struct_size = sizeof(request), .argc = sizeof(args) / sizeof(args[0]) - (mode == 0 ? 2 : 0),
        .argv = args, .stdin_data = source, .stdin_size = size,
        .resource_root = ON1X_CEPHYR_RESOURCE_ROOT, .archon_root = ON1X_CEPHYR_ARCHON_ROOT
    };
    cephyr_result *result = NULL;
    int status = cephyr_compile(&request, &result);
    size_t n = 0, d = 0;
    const char *output = cephyr_result_output(result, &n);
    const char *diagnostics = cephyr_result_diagnostics(result, &d);
    if (status < CEPHYR_OK || status > CEPHYR_INTERNAL_ERROR ||
        (status == CEPHYR_OK && (!result || (mode && !n))) ||
        !valid_text(diagnostics, d) || (status == CEPHYR_OK && !valid_text(output, n))) {
        status = CEPHYR_INTERNAL_ERROR;
        last_diagnostics = "invalid or oversized Cephyr text result";
    } else {
        if (status == CEPHYR_OK && n)
            last_output = on1x_text_slice(output, 0, (int64_t)n);
        if (d)
            last_diagnostics = on1x_text_slice(diagnostics, 0, (int64_t)d);
        else if (status != CEPHYR_OK)
            last_diagnostics = "Cephyr failed without diagnostics";
    }
    cephyr_result_destroy(result);
    return status;
}
