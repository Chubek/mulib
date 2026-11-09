#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#include "mulib-linebreak.h"
#include <assert.h>

static size_t allocation, fail_at, live;
static void *checked_malloc(size_t n) {
    void *p;
    if (++allocation == fail_at) return NULL;
    p = malloc(n); if (p) ++live; return p;
}
static void *checked_calloc(size_t n, size_t size) {
    void *p;
    if (++allocation == fail_at) return NULL;
    p = calloc(n, size); if (p) ++live; return p;
}
static void checked_free(void *p) { if (p) { assert(live); --live; free(p); } }
#define malloc checked_malloc
#define calloc checked_calloc
#define free checked_free
#define MULIB_IMPL_LINEBREAK
#include "mulib-linebreak.h"
#undef malloc
#undef calloc
#undef free

int main(void) {
    mulib_linebreak_options_t o;
    mulib_linebreak_result_t r = {0};
    size_t a, failure;
    mulib_linebreak_options_init(&o, 6);
    assert(!mulib_linebreak("old", 3, MULIB_LINEBREAK_GREEDY, &o, &r) && live == 1);
    for (a = 0; a < MULIB_LINEBREAK_UNICODE; ++a) {
        int succeeded = 0;
        for (failure = 1; failure <= 6; ++failure) {
            mulib_linebreak_line_t *old = r.lines;
            size_t old_count = r.count;
            int rc;
            allocation = 0; fail_at = failure;
            rc = mulib_linebreak("aa bb cc", 8, (mulib_linebreak_algorithm_t)a, &o, &r);
            if (rc) assert(rc == MULIB_LINEBREAK_MEMORY && r.lines == old && r.count == old_count);
            else succeeded = 1;
            assert(live == 1);
        }
        assert(succeeded);
    }
    fail_at = 0; mulib_linebreak_result_free(&r); assert(!live);
    return 0;
}
