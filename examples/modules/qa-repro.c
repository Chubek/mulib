/* Checked usage example for mulib-qa-repro.h. */
#define MULIB_IMPL_QA_REPRO
#include "mulib-qa-repro.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_qa_repro_t a, b;
    assert(mulib_qa_repro_init(&a, 42, 3, "clang", "amd64"));
    assert(mulib_qa_repro_init(&b, 42, 3, "clang", "amd64"));
    assert(mulib_qa_repro_hash(&a) == mulib_qa_repro_hash(&b));
    b.seed++;
    assert(mulib_qa_repro_hash(&a) != mulib_qa_repro_hash(&b));
    puts("PASS mulib-qa-repro");
    return 0;
}
