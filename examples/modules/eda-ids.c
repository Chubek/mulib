/* Checked usage example for mulib-eda-ids.h. */
#define MULIB_IMPL_EDA_IDS
#include "mulib-eda-ids.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_eda_id_pool_t p;
    mulib_eda_id_t a, b;
    assert(mulib_eda_id_pool_init(&p, 1));
    assert(mulib_eda_id_create(&p, 7, &a));
    assert(mulib_eda_id_valid(&p, a, 7));
    assert(!mulib_eda_id_valid(&p, a, 2));
    assert(mulib_eda_id_remove(&p, a));
    assert(!mulib_eda_id_valid(&p, a, 7));
    assert(mulib_eda_id_create(&p, 7, &b));
    assert(mulib_eda_id_index(a) == mulib_eda_id_index(b) && a != b);
    assert(!mulib_eda_id_remove(&p, a));
    mulib_eda_id_pool_free(&p);
    puts("PASS mulib-eda-ids");
    return 0;
}
