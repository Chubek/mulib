#define MULIB_IMPL_MICROJS
#define MULIB_IMPL_MINIVM
#include "mulib-microjs.h"
#include <stdio.h>
#include <string.h>
static mulib_microjs_status_t print_value(mulib_microjs_t *js,
    const mulib_microjs_value_t *args, size_t count,
    mulib_microjs_value_t *out, void *user) {
    (void)js; (void)user;
    if (count != 1 || args[0].type != MULIB_MICROJS_NUMBER) return MULIB_MICROJS_TYPE;
    printf("script total: %.0f\n", args[0].number);
    out->type = MULIB_MICROJS_UNDEFINED;
    return MULIB_MICROJS_OK;
}
int main(void) {
    const char *source =
        "function counter(start) { return function() { return ++start; }; }"
        "const next=counter(0); const totals={sum:0};"
        "for(let i=0;i<6;i++) { totals.sum += next(); }"
        "print(totals.sum); totals.sum === 21";
    mulib_microjs_t *js = mulib_microjs_create(NULL);
    mulib_microjs_value_t result = {0}; mulib_microjs_error_t error;
    if (!js) return 1;
    if (mulib_microjs_bind(js, "print", print_value, NULL) != MULIB_MICROJS_OK) {
        mulib_microjs_destroy(js); return 1;
    }
    if (mulib_microjs_eval(js, source, strlen(source), &result, &error) != MULIB_MICROJS_OK) {
        fprintf(stderr, "%zu:%zu: %s\n", error.line, error.column, error.message);
        mulib_microjs_destroy(js); return 1;
    }
    printf("MiniVM script result: %s\n", result.boolean ? "true" : "false");
    mulib_microjs_value_free(&result); mulib_microjs_destroy(js); return 0;
}
