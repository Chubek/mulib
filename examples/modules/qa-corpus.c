/* Checked usage example for mulib-qa-corpus.h. */
#define MULIB_IMPL_QA_CORPUS
#include "mulib-qa-corpus.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_qa_corpus_t c = {0};
    mulib_qa_case_t tc = {"rc-lowpass", 1234, 0};
    assert(mulib_qa_corpus_add(&c, tc));
    assert(mulib_qa_corpus_find(&c, "rc-lowpass")->input_fingerprint == 1234);
    assert(!mulib_qa_corpus_add(&c, tc));
    puts("PASS mulib-qa-corpus");
    return 0;
}
