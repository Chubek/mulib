#define MULIB_IMPL_MEMTEST
#include "mulib-memtest.h"
#include <assert.h>
int main(void) {
    uint64_t owned_ram[128]; mulib_memtest_result_t result;
    assert(!mulib_memtest_run(owned_ram, 128, 2, NULL, &result));
    assert(result.reads == 128 * 67 * 2 && !result.errors);
    assert(!mulib_memtest_march(owned_ram, 128, 2, NULL, &result));
    assert(result.reads == 128 * 5 * 2 && !result.errors);
    return 0;
}
