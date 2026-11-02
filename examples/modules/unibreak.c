#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#include "mulib-unibreak.h"
#include <assert.h>
int main(void) {
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    mulib_unibreak_boundary_t *b;
    size_t count, i;
    if (!u) { puts("ICU runtime unavailable"); return 0; }
    assert(!mulib_unibreak_find(u, MULIB_UNIBREAK_LINE, "en", "hello world\n", 12, &b, &count));
    for (i = 0; i < count; ++i) printf("byte %zu, rule %d\n", b[i].offset, b[i].status);
    free(b); mulib_uniproc_close(u); return 0;
}
