#define MULIB_IMPL_DISTFIELD
#include "mulib-distfield.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    const unsigned char mask[9] = {0,0,0, 0,255,0, 0,0,0};
    unsigned char *field;
    size_t i;
    assert(!mulib_distfield_generate(mask, 3, 3, 3, 2, &field));
    for (i = 0; i < 9; ++i) printf("%3u%c", field[i], i%3 == 2 ? '\n' : ' ');
    assert(field[4] > 127 && field[0] < 128); free(field); return 0;
}
