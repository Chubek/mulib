/* Private, build-time C11 process adapter for the independent MuLIB runtime. */
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-mune.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { INPUT_MAX = 512 * 1024, OUTPUT_MAX = 64 * 1024 };

static int evaluate(mulib_mune_state_t *state, const char *path,
                    mulib_mune_result_t **out)
{
    FILE *stream = fopen(path, "rb");
    char *bytes = stream ? malloc(INPUT_MAX + 1u) : NULL;
    size_t count = bytes ? fread(bytes, 1, INPUT_MAX + 1u, stream) : 0;
    int ok = bytes && !ferror(stream) && count <= INPUT_MAX;
    if (stream) fclose(stream);
    if (!ok) {
        fprintf(stderr, "on1x-mune-runner: cannot read bounded input %s\n", path);
    } else {
        mulib_mune_error_t error = {0};
        ok = mulib_mune_run_source(state, bytes, count, out, &error) == MULIB_MUNE_OK;
        if (!ok) {
            size_t line = 1, column = 1;
            for (size_t i = 0; i < error.offset && i < count; ++i) {
                if (bytes[i] == '\n') { ++line; column = 1; }
                else ++column;
            }
            fprintf(stderr, "%s:%zu:%zu: Mune: %s\n", path, line, column,
                    error.message ? error.message : "execution failed");
        }
    }
    free(bytes);
    return ok;
}

static int restrict_state(mulib_mune_state_t *state)
{
    const char *const blocked[] = {
        "os", "io", "debug", "package", "require", "dofile", "loadfile",
        "load", "coroutine", "print", "warn", "collectgarbage"
    };
    for (size_t i = 0; i < sizeof blocked / sizeof *blocked; ++i)
        if (mulib_mune_set_global(state, blocked[i], mulib_mune_nil()) != MULIB_MUNE_OK)
            return 0;
    mulib_mune_value_t math;
    return mulib_mune_get_global(state, "math", &math) == MULIB_MUNE_OK &&
        mulib_mune_set_field(state, math, "random", mulib_mune_nil()) == MULIB_MUNE_OK &&
        mulib_mune_set_field(state, math, "randomseed", mulib_mune_nil()) == MULIB_MUNE_OK;
}

struct output { char bytes[OUTPUT_MAX]; size_t length; int failed; };

static mulib_mune_status_t write_text(mulib_mune_state_t *state,
                                    const mulib_mune_value_t *args, size_t nargs,
                                    mulib_mune_result_t *result, void *user)
{
    (void)state;
    struct output *out = user;
    size_t length = 0;
    const char *text = nargs == 1 ? mulib_mune_value_string(args[0], &length) : NULL;
    if (!text || length > sizeof out->bytes - out->length) {
        out->failed = 1;  /* A caught Lua error must not admit partial output. */
        return MULIB_MUNE_LIMIT;
    }
    memcpy(out->bytes + out->length, text, length);
    out->length += length;
    return mulib_mune_result_push(result, mulib_mune_boolean(1))
        ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}

static int global_table(mulib_mune_state_t *state, const char *name, mulib_mune_value_t *out)
{
    return mulib_mune_make_table(state, out) == MULIB_MUNE_OK &&
        mulib_mune_set_global(state, name, *out) == MULIB_MUNE_OK;
}

static int set_data(mulib_mune_state_t *state, int argc, char **argv, int extended)
{
    mulib_mune_value_t data;
    if (!global_table(state, "data", &data)) return 0;
    mulib_mune_value_t context = {0}, targets = {0}, arguments = {0};
    if (extended &&
        (!global_table(state, "__on1x_context", &context) ||
         !global_table(state, "__on1x_targets", &targets) ||
         !global_table(state, "__on1x_arguments", &arguments))) return 0;
    for (int i = 0; i < argc;) {
        const char *tag = extended ? argv[i++] : "data";
        const char *key = argv[i++], *text = argv[i++];
        mulib_mune_value_t table = arguments, value;
        if (!strcmp(tag, "integer")) {
            char *end;
            errno = 0;
            intmax_t n = strtoimax(text, &end, 10);
            if (errno || end == text || *end || n < INT64_MIN || n > INT64_MAX) return 0;
            value = mulib_mune_integer((int64_t)n);
        } else if (!strcmp(tag, "boolean")) {
            if (strcmp(text, "true") && strcmp(text, "false")) return 0;
            value = mulib_mune_boolean(!strcmp(text, "true"));
        } else {
            if (!strcmp(tag, "data")) table = data;
            else if (!strcmp(tag, "context")) table = context;
            else if (!strcmp(tag, "target")) table = targets;
            else if (strcmp(tag, "string")) return 0;
            if (mulib_mune_make_string(state, text, strlen(text), &value) != MULIB_MUNE_OK) return 0;
        }
        if (mulib_mune_set_field(state, table, key, value) != MULIB_MUNE_OK) return 0;
    }
    return 1;
}

static int profile_output(mulib_mune_state_t *state, mulib_mune_value_t table)
{
    int opt = -1, debug = -1;
    size_t cursor = 0;
    mulib_mune_value_t key, value;
    int next;
    while ((next = mulib_mune_table_next(state, table, &cursor, &key, &value)) == 1) {
        size_t length;
        const char *name = mulib_mune_value_string(key, &length);
        if (name && length == 8 && !memcmp(name, "optimize", 8) &&
            value.type == MULIB_MUNE_INTEGER && value.as.integer >= 0 && value.as.integer <= 3)
            opt = (int)value.as.integer;
        else if (name && length == 5 && !memcmp(name, "debug", 5) && value.type == MULIB_MUNE_BOOLEAN)
            debug = !!value.as.boolean;
        else
            return 0;
    }
    return !next && opt >= 0 && debug >= 0 && printf("%d %d\n", opt, debug) > 0;
}

int main(int argc, char **argv)
{
    int extended = argc >= 2 && !strcmp(argv[1], "profile-context");
    if (argc < 4 || (!extended && strcmp(argv[1], "profile") && strcmp(argv[1], "render")) ||
        (argc - 4) % (extended ? 3 : 2)) {
        fputs("usage: on1x-mune-runner profile|render LIBRARY INPUT [KEY VALUE ...]\n"
              "       on1x-mune-runner profile-context LIBRARY INPUT [TAG KEY VALUE ...]\n", stderr);
        return 2;
    }
    int render = !strcmp(argv[1], "render"), ok = 0;
    const mulib_mune_runtime_options_t limits = { 8u * 1024 * 1024, 500000, 80, 2048 };
    struct output output = {{0}, 0, 0};
    mulib_mune_state_t *state = mulib_mune_state_create(&limits);
    mulib_mune_result_t *result = NULL;
    if (!state || !restrict_state(state) || !set_data(state, argc - 4, argv + 4, extended)) goto done;
    if (render && mulib_mune_bind(state, "__qmr_write_native", write_text, &output) != MULIB_MUNE_OK)
        goto done;
    if (!evaluate(state, argv[2], &result)) goto done;
    mulib_mune_result_destroy(result);
    result = NULL;
    if (!evaluate(state, argv[3], &result)) goto done;
    if (render) {
        ok = !output.failed && fwrite(output.bytes, 1, output.length, stdout) == output.length;
    } else if (mulib_mune_result_count(result) == 1) {
        ok = profile_output(state, mulib_mune_result_get(result, 0));
    }
    if (ok) ok = fflush(stdout) == 0;
done:
    mulib_mune_result_destroy(result);
    if (state) mulib_mune_state_destroy(state);
    if (!ok) fputs("on1x-mune-runner: invalid or exhausted build policy\n", stderr);
    return ok ? 0 : 1;
}
