#include <mulib-xatomics.h>
#include <stdlib.h>

static mulib_xatomic_u32_t allocations = MULIB_XATOMIC_INIT(0);
static mulib_xatomic_u32_t frees = MULIB_XATOMIC_INIT(0);

static void *tracked_malloc(size_t size) {
    void *out = malloc(size);
    if (out) (void)mulib_xatomic_u32_fetch_add(&allocations, 1);
    return out;
}

static void tracked_free(void *value) {
    (void)mulib_xatomic_u32_fetch_add(&frees, 1);
    free(value);
}

/* Exercise private native strong claims, not the single-threaded On1x
 * wrapper/cleanup ABI. Generated-C11 coverage links this as a separate TU. */
#define ON1X_SHARED_MALLOC tracked_malloc
#define ON1X_SHARED_FREE tracked_free
#include "../stdlib/container/shared.c"
#include <assert.h>

enum { workers = 6, iterations = 4000, rounds = 40 };
typedef struct {
    shared_block *block;
    mulib_xatomic_u32_t *gate;
    unsigned offset;
} stress_job;

static int worker(void *context) {
    stress_job *job = context;
    shared_block *block = job->block;
    while (!mulib_xatomic_u32_load(job->gate)) mulib_xthread_yield();
    for (unsigned i = 0; i < iterations; ++i) {
        unsigned index = (i * 19 + job->offset) % 64;
        assert(retain(block));
        const uint8_t *data = payload(block);
        assert(data[index] == (uint8_t)(index + 5));
        release(block);
    }
    release(block);
    return 0;
}

int main(void) {
    for (unsigned round = 0; round < rounds; ++round) {
        uint8_t data[64];
        for (size_t i = 0; i < sizeof data; ++i) data[i] = (uint8_t)(i + 5);
        on1x_unique *owner = on1x_shared_v1_new(data, sizeof data, NULL, true);
        assert(owner);
        shared_block *block = get(owner, true);
        /* Saturation is rejected without incrementing/wrapping. */
        mulib_xatomic_u32_store(&block->refs.atomic, UINT32_MAX);
        assert(!retain(block) && mulib_xatomic_u32_load(&block->refs.atomic) == UINT32_MAX);
        mulib_xatomic_u32_store(&block->refs.atomic, 1);

        mulib_xatomic_u32_t gate = MULIB_XATOMIC_INIT(0);
        stress_job jobs[workers];
        mulib_xthread_t threads[workers];
        for (unsigned j = 0; j < workers; ++j) {
            assert(retain(block));
            jobs[j] = (stress_job){block, &gate, j + round};
            assert(mulib_xthread_create(&threads[j], worker, &jobs[j]) == MULIB_XTHREAD_OK);
        }
        on1x_unique_drop(owner); /* exactly one worker performs the final free */
        assert(on1x_unique_live_allocations() == 0);
        assert(mulib_xatomic_u32_load(&frees) == round);
        mulib_xatomic_u32_store(&gate, 1);
        for (unsigned j = 0; j < workers; ++j) {
            int result = -1;
            assert(mulib_xthread_join(threads[j], &result) == MULIB_XTHREAD_OK && result == 0);
        }
        assert(mulib_xatomic_u32_load(&allocations) == round + 1);
        assert(mulib_xatomic_u32_load(&frees) == round + 1);
    }
}
