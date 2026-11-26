#include <cephyr/Cephyr.h>
#include "on1x_runtime.h"
#include <string.h>

/* Immutable On1x text snapshots outlive replacement of these slots. */
static _Thread_local const char *last_output = "";
static _Thread_local const char *last_diagnostics = "";
enum { text_limit = 4 * 1024 * 1024 };
enum {
    mode_check = 0, mode_mir = 1, mode_assembly = 2, mode_preprocess = 3,
    mode_parse = 4, mode_semantics = 5
};

const char *on1x_cephyr_output_v1(void) { return last_output; }
const char *on1x_cephyr_diagnostics_v1(void) { return last_diagnostics; }

static int valid_text(const char *data, size_t n) {
    return n == 0 || (data && n <= text_limit && !memchr(data, 0, n));
}

int64_t on1x_cephyr_analyze_v1(const char *source, int64_t mode) {
    last_output = "";
    last_diagnostics = "";
    if (!source || mode < mode_check || mode > mode_semantics) {
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
    static const char *const stages[] = {
        [mode_check] = "-fsyntax-only", [mode_mir] = "--emit-mir",
        [mode_assembly] = "-S", [mode_preprocess] = "-E"
    };
    const char *args[11] = {"--config", ON1X_CEPHYR_CONFIG, "--target=x86_64-linux-gnu",
                            "--no-sandslab", "--no-factir", "-x", "c"};
    size_t argc = 7;
    if (mode <= mode_preprocess)
        args[argc++] = stages[mode];
    args[argc++] = "-";
    if (mode != mode_check) {
        args[argc++] = "-o";
        args[argc++] = "-";
    }
    cephyr_request request = {
        .struct_size = sizeof(request),
        .argc = argc,
        .argv = args, .stdin_data = source, .stdin_size = size,
        .resource_root = ON1X_CEPHYR_RESOURCE_ROOT, .archon_root = ON1X_CEPHYR_ARCHON_ROOT
    };
    cephyr_result *result = NULL;
    int status;
    if (mode == mode_parse)
        status = cephyr_parse_c11(&request, CEPHYR_AST_SEXP, &result);
    else if (mode == mode_semantics)
        status = cephyr_emit_interface(&request, &result);
    else
        status = cephyr_compile(&request, &result);
    size_t n = 0, d = 0;
    const char *output = cephyr_result_output(result, &n);
    const char *diagnostics = cephyr_result_diagnostics(result, &d);
    /* A translation unit can have no preprocessed tokens. */
    const int needs_output = mode != mode_check && mode != mode_preprocess;
    if (status < CEPHYR_OK || status > CEPHYR_INTERNAL_ERROR ||
        (status == CEPHYR_OK && (!result || (needs_output && !n))) ||
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
