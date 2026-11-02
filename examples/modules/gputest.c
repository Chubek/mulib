#define MULIB_IMPL_GPUTEST
#include "mulib-gputest.h"
#include <assert.h>
#include <stddef.h>
/* Mock driver demonstrates sequencing only, not a real GPU workload. */
static int workload(void *ctx, uint32_t seed, uint32_t count) {
    (void)ctx; (void)seed; (void)count; return 0;
}
static int wait_done(void *ctx, uint32_t timeout) { (void)ctx; (void)timeout; return 0; }
static void cleanup(void *ctx) { ++*(unsigned *)ctx; }
int main(void) {
    unsigned cleaned = 0;
    uint32_t reference[64]; unsigned i;
    mulib_gputest_backend_t driver = {&cleaned, workload, wait_done, workload, NULL, cleanup};
    mulib_gputest_config_t config = {10, 1024, 500, 0}; mulib_gputest_result_t result;
    assert(!mulib_gputest_run(&driver, &config, &result));
    assert(result.completed == 10 && cleaned == 1);
    /* CPU reference validation only. Compile mulib_gputest_shader() in your
     * real compute driver and replace this array with synchronized GPU readback. */
    for (i = 0; i < 64; ++i) reference[i] = mulib_gputest_reference(123, i, 128);
    assert(!mulib_gputest_verify(reference, 64, 123, 128));
    return 0;
}
