#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNINAME
#include "mulib-uniname.h"
#include <assert.h>
int main(void) {
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    uint32_t cp;
    char *name;
    if (!u) { puts("ICU runtime unavailable"); return 0; }
    assert(!mulib_uniname_lookup(u, "GREEK SMALL LETTER PI", 0, &cp));
    name = mulib_uniname_name(u, cp, 0); assert(name);
    printf("U+%04X %s\n", (unsigned)cp, name);
    free(name); mulib_uniproc_close(u); return 0;
}
