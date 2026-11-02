#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_BIDI
#include "mulib-bidi.h"
#include <assert.h>
int main(void) {
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    mulib_bidi_result_t result = {0};
    size_t i;
    if (!u) { puts("ICU runtime unavailable"); return 0; }
    assert(!mulib_bidi_resolve(u, "abc \xd7\x90\xd7\x91", 8, -1, &result));
    for (i = 0; i < result.count; ++i)
        printf("bytes %zu..%zu, level %u\n", result.runs[i].start,
               result.runs[i].start + result.runs[i].length, result.runs[i].level);
    mulib_bidi_destroy(&result); mulib_uniproc_close(u); return 0;
}
