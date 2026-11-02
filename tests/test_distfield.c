#define MULIB_IMPL_DISTFIELD
#include "mulib-distfield.h"
#include <assert.h>

/* Independent quadratic reference, including outside-of-image background. */
static unsigned char reference(const unsigned char *mask, size_t w, size_t h, size_t x, size_t y, double spread) {
    size_t xx, yy;
    int inside = mask[y*w+x] >= 128;
    double distance = 1e10, encoded;
    for (yy = 0; yy < h; ++yy) for (xx = 0; xx < w; ++xx) if ((mask[yy*w+xx] >= 128) != inside) {
        double dx = (double)x-(double)xx, dy = (double)y-(double)yy, d = sqrt(dx*dx+dy*dy);
        if (d < distance) distance = d;
    }
    if (inside) {
        double edge = fmin(fmin((double)(x+1), (double)(w-x)), fmin((double)(y+1), (double)(h-y)));
        if (edge < distance) distance = edge;
    }
    encoded = .5 + (distance-.5)*(inside ? 1 : -1)/(2*spread);
    return (unsigned char)floor(255*fmax(0, fmin(1, encoded))+.5);
}
int main(void) {
    unsigned char mask[63], *field = NULL;
    unsigned state = 12345;
    size_t i, x, y;
    int trial;
    for (trial = 0; trial < 100; ++trial) {
        for (i = 0; i < sizeof mask; ++i) { state = state*1664525u+1013904223u; mask[i] = trial == 0 ? 0 : trial == 1 ? 255 : (unsigned char)(state >> 24); }
        assert(!mulib_distfield_generate(mask, 9, 7, 9, 4, &field));
        for (y = 0; y < 7; ++y) for (x = 0; x < 9; ++x) assert(field[y*9+x] == reference(mask, 9, 7, x, y, 4));
        free(field); field = NULL;
    }
    assert(!mulib_distfield_generate(NULL, 0, 0, 0, 1, &field) && !field);
    {
        unsigned char empty = 0;
        assert(!mulib_distfield_generate(&empty, 1, 1, 1, 1e100, &field) && field[0] == 0);
        free(field); field = NULL;
    }
    assert(mulib_distfield_generate(mask, SIZE_MAX, 1, SIZE_MAX, 1, &field) == -1);
    assert(mulib_distfield_generate(mask, 9, 7, 8, 1, &field) == -1);
    assert(mulib_distfield_generate(mask, 9, 7, 9, NAN, &field) == -1);
    assert(mulib_distfield_generate(mask, 9, 7, 9, 0, &field) == -1);
    {
        unsigned char padded[6] = {255,0,99, 0,255,99};
        unsigned char tight[4] = {255,0,0,255};
        size_t bits;
        assert(!mulib_distfield_generate(padded, 2, 2, 3, 2, &field));
        for (y = 0; y < 2; ++y) for (x = 0; x < 2; ++x) assert(field[y*2+x] == reference(tight, 2, 2, x, y, 2));
        free(field); field = NULL;
        /* Exhaust every binary 3x3 mask, including all one-pixel edge cases. */
        for (bits = 0; bits < 512; ++bits) {
            for (i = 0; i < 9; ++i) mask[i] = bits & ((size_t)1 << i) ? 255 : 0;
            assert(!mulib_distfield_generate(mask, 3, 3, 3, 1.25, &field));
            for (y = 0; y < 3; ++y) for (x = 0; x < 3; ++x) assert(field[y*3+x] == reference(mask, 3, 3, x, y, 1.25));
            free(field); field = NULL;
        }
    }
    assert(mulib_distfield_distance(255, 4) == 4 && mulib_distfield_distance(0, 4) == -4);
    assert(mulib_distfield_coverage(.5, .1) == .5 && mulib_distfield_coverage(0, .1) == 0 && mulib_distfield_coverage(1, .1) == 1);
    return 0;
}
