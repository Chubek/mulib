#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#include "mulib-uniproc.h"
#include <assert.h>
int main(void) {
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    char *s;
    if (!u) { puts("ICU runtime unavailable"); return 0; }
    s = mulib_uniproc_case_map(u, MULIB_UNIPROC_FOLD, NULL, "Stra\xc3\x9f" "e", 7, NULL);
    assert(s && !strcmp(s, "strasse")); puts(s); free(s);
    mulib_uniproc_close(u); return 0;
}
