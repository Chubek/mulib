/* Fault-injection implementation TU for the canonical JSON kernel. The normal
 * runtime recipe is included exactly once; only kernel allocation is replaced. */
#include <stddef.h>
#include <stdlib.h>

static int fail_allocations;
void fail_json_allocations(void) { fail_allocations = 1; }

static void* test_malloc(size_t size) {
    return fail_allocations ? NULL : malloc(size);
}
static void* test_calloc(size_t count, size_t size) {
    return fail_allocations ? NULL : calloc(count, size);
}
static void* test_realloc(void* pointer, size_t size) {
    return fail_allocations ? NULL : realloc(pointer, size);
}

#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#include "../../runtime/on1x_mulib_impl.c"
