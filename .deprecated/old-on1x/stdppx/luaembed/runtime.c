/* Mune's implementation switches belong exclusively to the generated planner TU. */
#include "runtime.h"
#include <mulib-mune.h>
#include <stdio.h>
#include <string.h>

#include "luaembed_resources.h"

enum { LL_SOURCE = 65536, LL_OUTPUT = 524288, LL_INPUT = 4194304 };
static char captured[LL_OUTPUT + 1], failure[1024];
static size_t captured_length;
static bool poisoned;
static int64_t failure_offset;
static int64_t sequence;
static size_t total_source, total_output;

const char *on1x_luaembed_message(const char *key)
{
    size_t i;
    if (key)
        for (i = 0; i < sizeof ll_messages / sizeof *ll_messages; ++i)
            if (!strcmp(key, ll_messages[i].key)) return ll_messages[i].text;
    for (i = 0; i < sizeof ll_messages / sizeof *ll_messages; ++i)
        if (!strcmp("internal", ll_messages[i].key)) return ll_messages[i].text;
    return "";
}

static bool bounded(const char *value, size_t limit, size_t *length)
{
    size_t i;
    if (!value) return false;
    for (i = 0; i <= limit; ++i)
        if (!value[i]) { *length = i; return true; }
    return false;
}

static void fail(const char *key, const mulib_mune_error_t *error)
{
    const char *text = on1x_luaembed_message(key);
    size_t used = (size_t)snprintf(failure, sizeof failure, "%s", text);
    failure_offset = error && error->offset <= LL_SOURCE ? (int64_t)error->offset : 0;
    if (error && error->message && used + 2 < sizeof failure) {
        const unsigned char *p = (const unsigned char *)error->message;
        size_t count = 0;
        failure[used++] = ':'; failure[used++] = ' ';
        while (*p && used + 5 < sizeof failure && count++ < 512) {
            unsigned char c = *p++;
            if (c >= 32 && c <= 126) failure[used++] = (char)c;
            else {
                static const char hex[] = "0123456789abcdef";
                failure[used++] = '\\'; failure[used++] = 'x';
                failure[used++] = hex[c >> 4]; failure[used++] = hex[c & 15];
            }
        }
        failure[used] = 0;
    }
    captured_length = 0; captured[0] = 0;
}

static mulib_mune_status_t write_capture(mulib_mune_state_t *state,
    const mulib_mune_value_t *args, size_t count, mulib_mune_result_t *result, void *user)
{
    size_t size = 0;
    const char *text = count == 1 ? mulib_mune_value_string(args[0], &size) : NULL;
    (void)state; (void)result; (void)user;
    if (poisoned || !text || size > LL_OUTPUT - captured_length || memchr(text, 0, size)) {
        poisoned = true;
        return MULIB_MUNE_LIMIT;
    }
    memcpy(captured + captured_length, text, size);
    captured_length += size;
    captured[captured_length] = 0;
    return MULIB_MUNE_OK;
}

static bool string_global(mulib_mune_state_t *state, const char *name,
                          const char *text, size_t length)
{
    mulib_mune_value_t value;
    return mulib_mune_make_string(state, text, length, &value) == MULIB_MUNE_OK &&
           mulib_mune_set_global(state, name, value) == MULIB_MUNE_OK;
}

static bool restrict_state(mulib_mune_state_t *state)
{
    static const char *const blocked[] = {
        "os", "io", "debug", "package", "require", "dofile", "loadfile",
        "load", "coroutine", "print", "warn", "collectgarbage"
    };
    mulib_mune_value_t math;
    size_t i;
    for (i = 0; i < sizeof blocked / sizeof *blocked; ++i)
        if (mulib_mune_set_global(state, blocked[i], mulib_mune_nil()) != MULIB_MUNE_OK)
            return false;
    return mulib_mune_get_global(state, "math", &math) == MULIB_MUNE_OK &&
        mulib_mune_set_field(state, math, "random", mulib_mune_nil()) == MULIB_MUNE_OK &&
        mulib_mune_set_field(state, math, "randomseed", mulib_mune_nil()) == MULIB_MUNE_OK;
}

bool on1x_luaembed_eval(const char *source, const char *location,
                      const char *label, const char *input)
{
    const mulib_mune_parse_options_t parse_limits = { LL_SOURCE, 16384, 32768, 8192, 500000 };
    const mulib_mune_runtime_options_t runtime_limits = { 16u * 1024 * 1024, 500000, 64, 4096 };
    mulib_mune_chunk_t *chunk = NULL;
    mulib_mune_state_t *state = NULL;
    mulib_mune_result_t *result = NULL;
    mulib_mune_value_t messages;
    mulib_mune_error_t error = {0};
    size_t n, at, name, tree, i;
    bool ok = false;
    captured_length = 0; captured[0] = failure[0] = 0;
    failure_offset = 0; poisoned = false;
    if (!bounded(source, LL_SOURCE, &n) ||
        !bounded(location, 32768, &at) || !bounded(label, 128, &name) ||
        !bounded(input, LL_INPUT, &tree)) { fail("input", NULL); return false; }
    if (sequence >= 64 || n > 1048576u - total_source) {
        fail("expansions", NULL); return false;
    }
    ++sequence; total_source += n;
    /* Validate the user's source before running even the trusted Lua library. */
    chunk = mulib_mune_parse(source, n, &parse_limits, &error);
    if (!chunk) { fail("lua", &error); goto done; }
    state = mulib_mune_state_create(&runtime_limits);
    if (!state || !restrict_state(state) ||
        mulib_mune_bind(state, "__llembed_write", write_capture, NULL) != MULIB_MUNE_OK ||
        !string_global(state, "__llembed_location", location, at) ||
        !string_global(state, "__llembed_label", label, name) ||
        !string_global(state, "__llembed_input", input, tree) ||
        mulib_mune_set_global(state, "__llembed_serial", mulib_mune_integer(sequence)) != MULIB_MUNE_OK ||
        mulib_mune_make_table(state, &messages) != MULIB_MUNE_OK ||
        mulib_mune_set_global(state, "__llembed_messages", messages) != MULIB_MUNE_OK) {
        fail("memory", NULL); goto done;
    }
    for (i = 0; i < sizeof ll_messages / sizeof *ll_messages; ++i) {
        mulib_mune_value_t value;
        if (mulib_mune_make_string(state, ll_messages[i].text, strlen(ll_messages[i].text), &value) != MULIB_MUNE_OK ||
            mulib_mune_set_field(state, messages, ll_messages[i].key, value) != MULIB_MUNE_OK) {
            fail("memory", NULL); goto done;
        }
    }
    if (mulib_mune_run_source(state, (const char *)ll_library, sizeof ll_library - 1,
                             &result, &error) != MULIB_MUNE_OK) {
        fail("lua", &error); goto done;
    }
    mulib_mune_result_destroy(result); result = NULL;
    if (mulib_mune_run(state, chunk, &result, &error) != MULIB_MUNE_OK || poisoned) {
        fail(poisoned ? "output" : "lua", &error); goto done;
    }
    ok = true;
done:
    mulib_mune_result_destroy(result);
    /* Finalizers are allowed to run only against this still-live capture.
     * They cannot publish output after a failed run. Recheck sticky overflow. */
    if (state) mulib_mune_state_destroy(state);
    mulib_mune_chunk_destroy(chunk);
    if (poisoned) { fail("output", NULL); ok = false; }
    if (captured_length > LL_INPUT - total_output) { fail("expansions", NULL); ok = false; }
    if (ok) total_output += captured_length;
    if (!ok) { captured_length = 0; captured[0] = 0; }
    return ok;
}

const char *on1x_luaembed_output(void) { return captured; }
const char *on1x_luaembed_error(void) { return failure; }
int64_t on1x_luaembed_offset(void) { return failure_offset; }
