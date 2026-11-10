/* Focused executable regression for mulib-eda-ids.h. */
#define MULIB_IMPL_EDA_IDS
#include "mulib-eda-ids.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static void retirement(void) {
    mulib_eda_id_pool_t p = {0};
    mulib_eda_id_t a, b, c;
    assert(mulib_eda_id_pool_init(&p, 1));
    assert(mulib_eda_id_create(&p, 7, &a));
    /* Reach the boundary without sixteen million create/remove cycles. */
    p.generation[0] = 0xfffffeu;
    a = (UINT64_C(7) << 56) | (UINT64_C(0xfffffe) << 32) | 1;
    assert(mulib_eda_id_valid(&p, a, 7));
    assert(mulib_eda_id_remove(&p, a));
    assert(mulib_eda_id_create(&p, 7, &b));
    assert(mulib_eda_id_index(b) == 0 && p.length == 1);
    assert(((b >> 32) & 0xffffffu) == 0xffffffu);
    assert(!mulib_eda_id_valid(&p, a, 0));
    assert(mulib_eda_id_valid(&p, b, 7));
    assert(mulib_eda_id_remove(&p, b));
    assert(p.generation[0] == 0x1000000u);
    assert(mulib_eda_id_create(&p, 8, &c));
    assert(mulib_eda_id_index(c) == 1 && p.length == 2);
    assert(!mulib_eda_id_valid(&p, a, 0) && !mulib_eda_id_valid(&p, b, 0));
    assert(!mulib_eda_id_remove(&p, b));
    assert(mulib_eda_id_valid(&p, c, 8));
    mulib_eda_id_pool_free(&p);
}

static void reuse_model(void) {
    mulib_eda_id_pool_t p = {0};
    mulib_eda_id_t current[32] = {0}, stale[32] = {0};
    uint32_t seed = 12345;
    assert(mulib_eda_id_pool_init(&p, 0));
    for (size_t step = 0; step < 4096; step++) {
        seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
        size_t slot = (seed >> 16) % 32;
        uint8_t kind = (uint8_t)(slot + 1);
        if (current[slot]) {
            assert(mulib_eda_id_remove(&p, current[slot]));
            stale[slot] = current[slot];
            current[slot] = 0;
        } else {
            assert(mulib_eda_id_create(&p, kind, &current[slot]));
        }
        for (size_t i = 0; i < 32; i++) {
            assert(!mulib_eda_id_valid(&p, stale[i], 0));
            if (current[i]) {
                assert(mulib_eda_id_valid(&p, current[i], (uint8_t)(i + 1)));
                assert(!mulib_eda_id_valid(&p, current[i], (uint8_t)(i + 2)));
                for (size_t j = 0; j < i; j++)
                    assert(!current[j] || current[i] != current[j]);
            }
        }
    }
    mulib_eda_id_pool_free(&p);
}

int main(void) {
    mulib_eda_id_pool_t p = {0};
    mulib_eda_id_t a, b, unchanged = 42;
    assert(!mulib_eda_id_pool_init(NULL, 1));
    assert(!mulib_eda_id_pool_init(&p, SIZE_MAX));
    assert(!p.generation && !p.active && !p.kind && !p.length && !p.capacity);
    assert(mulib_eda_id_pool_init(&p, 1));
    assert(!mulib_eda_id_create(&p, 0, &unchanged) && unchanged == 42);
    assert(!mulib_eda_id_create(&p, 7, NULL));
    assert(mulib_eda_id_create(&p, 7, &a));
    assert(mulib_eda_id_valid(&p, a, 7));
    assert(!mulib_eda_id_valid(&p, a, 2));
    assert(!mulib_eda_id_valid(&p, 0, 0));
    assert(mulib_eda_id_index(0) == UINT32_MAX);
    assert(mulib_eda_id_remove(&p, a));
    assert(!mulib_eda_id_valid(&p, a, 7));
    assert(mulib_eda_id_create(&p, 7, &b));
    assert(mulib_eda_id_index(a) == mulib_eda_id_index(b) && a != b);
    assert(!mulib_eda_id_remove(&p, a));
    mulib_eda_id_pool_free(&p);
    mulib_eda_id_pool_free(&p);
    retirement();
    reuse_model();
    puts("PASS mulib-eda-ids");
    return 0;
}
