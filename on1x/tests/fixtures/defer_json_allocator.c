/* One canonical implementation TU; count every successful JSON allocation. */
#include "on1x_runtime.h"
#include <stdlib.h>
static int64_t live;
int64_t defer_json_blocks(void) { return live; }
static void *count_malloc(size_t n) {
    void *p = malloc(n); if (p) ++live; return p;
}
static void *count_calloc(size_t n, size_t size) {
    void *p = calloc(n, size); if (p) ++live; return p;
}
static void *count_realloc(void *old, size_t n) {
    int had_old = old != NULL;
    void *p;
    if (!n) { if (had_old) --live; free(old); return NULL; }
    p = realloc(old, n); if (p && !had_old) ++live; return p;
}
static void count_free(void *p) {
    if (p) { if (!live) abort(); --live; } free(p);
}
#define malloc count_malloc
#define calloc count_calloc
#define realloc count_realloc
#define free count_free
#include "../../runtime/on1x_mulib_impl.c"
