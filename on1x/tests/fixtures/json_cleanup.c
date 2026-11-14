/* A counting test double for lexical-drop code generation. Real MuLIB JSON
 * integration is exercised separately; this is not an FFI safety proof. */
#include "on1x_runtime.h"
#include <stdlib.h>

struct mulib_json { int64_t depth; };
static int64_t live;

on1x_json *on1x_json_parse_or_die(const char *text) {
    on1x_json *value = malloc(sizeof *value);
    (void)text;
    if (!value) abort();
    value->depth = ++live;
    return value;
}

void on1x_json_drop(on1x_json *value) {
    /* Owners must drop exactly once, in reverse lexical declaration order. */
    if (!value || value->depth != live) abort();
    --live;
    free(value);
}

int64_t live_roots(void) { return live; }
