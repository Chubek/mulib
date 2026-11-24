/* Standalone: cc -std=c11 -Wall -Wextra -Wpedantic -Werror
 * -Wno-misleading-indentation -Ikernels tests/test-gfx-path.c -lm -o test */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fail each allocation site without modifying the production allocator API. */
static long allocation_budget = -1;
static void *path_test_malloc(size_t n)
{
    if (allocation_budget == 0)
        return NULL;
    if (allocation_budget > 0)
        --allocation_budget;
    return malloc(n);
}
static void *path_test_realloc(void *p, size_t n)
{
    if (allocation_budget == 0)
        return NULL;
    if (allocation_budget > 0)
        --allocation_budget;
    return realloc(p, n);
}
#define malloc path_test_malloc
#define realloc path_test_realloc
#include "mulib-glx-antialias.h"
#include "mulib-glx-svg.h"
#include "mulib-glx-stroke.h"
#include "mulib-glx-transform.h"
#include "mulib-glx-tessellate.h"
#include "mulib-glx-shape.h"
#include "mulib-glx-paint.h"
#include "mulib-glx-mesh.h"
#include "mulib-glx-filter.h"
#undef malloc
#undef realloc

static void test_path_validation(void)
{
    mulib_glx_path_t p = {0};
    assert(!mulib_glx_path_move(NULL, 0, 0));
    assert(!mulib_glx_path_move(&p, NAN, 0));
    assert(!mulib_glx_path_line(&p, 0, INFINITY));
    assert(!mulib_glx_path_move(&p, 100000001, 0));
    assert(!mulib_glx_path_close(&p));
    assert(!mulib_glx_path_cubic(&p, (mgx_point){0, 0},
                               (mgx_point){0, 0}, (mgx_point){0, 0}));
    assert(mulib_glx_path_move(&p, 1, 2));
    assert(!mulib_glx_path_quad(&p, (mgx_point){NAN, 0}, (mgx_point){0, 0}));
    assert(!mulib_glx_path_cubic(&p, (mgx_point){0, 0},
                               (mgx_point){100000001, 0}, (mgx_point){0, 0}));
    assert(!mulib_glx_path_cubic_tol(&p, (mgx_point){0, 0},
                                   (mgx_point){1, 1}, (mgx_point){2, 2}, 0));
    assert(p.n == 1 && p.first.x == 1 && p.first.y == 2 && p.open);
    assert(!mulib_glx_path_arc(&p, NAN, 0, 1, 0, 1));
    assert(!mulib_glx_path_arc(&p, 0, 0, INFINITY, 0, 1));
    assert(!mulib_glx_path_arc(&p, 0, 0, 1, -1e308, 1e308));
    /* Failure after emitting some arc vertices is also atomic. */
    assert(!mulib_glx_path_arc(&p, 99999999, 0, 2, 3.141592653589793, 0));
    assert(p.n == 1 && p.first.x == 1 && p.first.y == 2 && p.open);
    assert(mulib_glx_path_close(&p));
    assert(!p.open && p.n == 2);
    assert(mulib_glx_path_line(&p, 3, 4));
    assert(p.move[2] && p.first.x == 3 && p.first.y == 4);
    mulib_glx_path_free(&p);
    assert(!p.p && !p.move && !p.n && !p.cap && !p.mc);
    p.n = SIZE_MAX;
    assert(!mulib_glx_path_add(&p, 0, 0, 0));
    assert(!mulib_glx_path_valid(&p));
    p.n = 0;
}

static mgx_point cubic_point(mgx_point a, mgx_point b, mgx_point c,
                             mgx_point d, double t)
{
    double u = 1 - t;
    return (mgx_point){
        u*u*u*a.x + 3*u*u*t*b.x + 3*u*t*t*c.x + t*t*t*d.x,
        u*u*u*a.y + 3*u*u*t*b.y + 3*u*t*t*c.y + t*t*t*d.y};
}

static void assert_cubic_error(mgx_point a, mgx_point b,
                               mgx_point c, mgx_point d, double tolerance)
{
    mulib_glx_path_t p = {0};
    assert(mulib_glx_path_move(&p, a.x, a.y));
    assert(mulib_glx_path_cubic_tol(&p, b, c, d, tolerance));
    assert(p.n >= 2 && p.p[p.n - 1].x == d.x && p.p[p.n - 1].y == d.y);
    for (unsigned i = 0; i <= 4096; ++i) {
        mgx_point v = cubic_point(a, b, c, d, (double)i / 4096);
        double distance = INFINITY;
        for (size_t j = 1; j < p.n; ++j)
            distance = fmin(distance, mgx_distseg(v.x, v.y, p.p[j - 1], p.p[j]));
        assert(distance <= tolerance + 1e-8);
    }
    mulib_glx_path_free(&p);
}

static void test_adaptive_curves(void)
{
    mulib_glx_path_t p = {0}, q = {0};
    assert(mulib_glx_path_move(&p, 0, 0));
    assert(mulib_glx_path_cubic(&p, (mgx_point){200, 0},
                               (mgx_point){800, 0}, (mgx_point){1000, 0}));
    assert(p.n == 2); /* Long straight curves need one segment, not 64+. */
    mulib_glx_path_free(&p);
    assert_cubic_error((mgx_point){0, 0}, (mgx_point){0, 300},
                       (mgx_point){400, -100}, (mgx_point){300, 200}, .25);
    assert_cubic_error((mgx_point){0, 0}, (mgx_point){100, 300},
                       (mgx_point){-100, 300}, (mgx_point){0, 0}, .05);
    assert_cubic_error((mgx_point){0, 0}, (mgx_point){500, 0},
                       (mgx_point){-500, 0}, (mgx_point){0, 0}, .1);
    assert(mulib_glx_path_move(&p, 0, 0));
    assert(mulib_glx_path_move(&q, 0, 0));
    assert(!mulib_glx_path_cubic_tol(&p, (mgx_point){0, 1},
                                   (mgx_point){1, 0}, (mgx_point){1, 1},
                                   DBL_TRUE_MIN));
    assert(p.n == 1 && p.open && p.first.x == 0 && p.first.y == 0);
    assert(mulib_glx_path_quad_tol(&p, (mgx_point){50, 100},
                                  (mgx_point){100, 0}, .5));
    assert(mulib_glx_path_quad_tol(&q, (mgx_point){50, 100},
                                  (mgx_point){100, 0}, .05));
    assert(q.n > p.n);
    mulib_glx_path_free(&p);
    mulib_glx_path_free(&q);
    assert(mulib_glx_path_arc(&p, 0, 0, 1000, 0, 6.283185307179586));
    for (size_t i = 1; i < p.n; ++i) {
        mgx_point middle = mulib_glx_path_midpoint(p.p[i - 1], p.p[i]);
        assert(1000 - hypot(middle.x, middle.y) <= .25 + 1e-9);
    }
    mulib_glx_path_free(&p);
}

static void test_svg(void)
{
    static const char *const bad[] = {
        "L0 0", "Z", "M", "M0", "M0 0 1", "M0 0L1", "M0 0Q1 2 3",
        "M0 0C1 2 3 4 5", "M0 0A1 1 0 0 0 2 2", "MNaN 0",
        "MInf 0", "M0x10 0", "M1e 0", "M1e+ 0", "M1e9999 0",
        "M100000001 0", "M0 0C0 0 100000001 0 0 0",
        "M,0 0", "M0,,0", "M0 0,", "M0 0L,1 1", "M0 0Z 1 1",
        "M. 0", "M+ 0", "M0 0?", "M0 0 m1e308 1e308"
    };
    mulib_glx_path_t p = {0};
    const char *number = " \r\n,-.125e+2x";
    double value = 0;
    assert(mgx_svg_number(&number, &value) && value == -12.5 && *number == 'x');
    assert(!mgx_svg_number(NULL, &value));
    assert(!mulib_glx_svg_path(NULL, "M0 0"));
    assert(!mulib_glx_svg_path(&p, NULL));
    assert(mulib_glx_svg_path(&p, " \r\n\t"));
    assert(mulib_glx_svg_path(&p, "m1 2 3 4h5v-2z l2 0"));
    assert(p.n == 7);
    assert(p.p[0].x == 1 && p.p[0].y == 2 && p.move[0]);
    assert(p.p[1].x == 4 && p.p[1].y == 6 && !p.move[1]);
    assert(p.p[2].x == 9 && p.p[2].y == 6);
    assert(p.p[3].x == 9 && p.p[3].y == 4);
    assert(p.p[4].x == 1 && p.p[4].y == 2);
    assert(p.move[5] && p.p[5].x == 1 && p.p[5].y == 2);
    assert(p.p[6].x == 3 && p.p[6].y == 2);
    mulib_glx_path_free(&p);
    assert(mulib_glx_svg_path(&p, "M.5-.5L1e1 2.E+0,3.5 4z"));
    assert(p.n == 4 && p.p[1].x == 10 && p.p[1].y == 2);
    assert(mulib_glx_svg_path(&p, "M0 0q1 2 3 4c1 2 3 4 5 6z"));
    assert(!p.open);
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        size_t saved = p.n;
        mgx_point first = p.first;
        int open = p.open;
        mgx_point *copy = (mgx_point *)malloc(saved * sizeof *copy);
        assert(copy);
        memcpy(copy, p.p, saved * sizeof *copy);
        assert(!mulib_glx_svg_path(&p, bad[i]));
        assert(p.n == saved && p.open == open);
        assert(p.first.x == first.x && p.first.y == first.y);
        assert(!memcmp(copy, p.p, saved * sizeof *copy));
        free(copy);
    }
    {
        const char bytes[] = {'M', '1', ' ', '2', 'L', '3', ' ', '4'};
        size_t saved = p.n;
        assert(mulib_glx_svg_path_n(&p, bytes, sizeof bytes));
        assert(p.n == saved + 2);
        assert(!mulib_glx_svg_path_n(&p, "M0\0 0", 5));
        assert(!mulib_glx_svg_path_n(&p, "", MULIB_GLX_SVG_MAX_BYTES + 1u));
    }
    {
        char too_long[160];
        memset(too_long, '0', sizeof too_long);
        too_long[0] = 'M';
        too_long[sizeof too_long - 3] = ' ';
        too_long[sizeof too_long - 2] = '0';
        too_long[sizeof too_long - 1] = '\0';
        assert(!mulib_glx_svg_path(&p, too_long));
    }
    mulib_glx_path_free(&p);
}

static void reference_fill(mgx_surface *s, const mulib_glx_path_t *p,
                           uint32_t color, int evenodd, unsigned samples)
{
    for (int y = 0; y < s->h; ++y)
        for (int x = 0; x < s->w; ++x) {
            double coverage = mulib_glx_raster_coverage(p, x, y, evenodd, samples);
            if (coverage)
                mgx_pixel(s, x, y, mgx_alpha(color, coverage));
        }
}

static void compare_fill(const mulib_glx_path_t *p)
{
    mgx_surface actual = {0}, expected = {0};
    assert(mgx_surface_init(&actual, 31, 29));
    assert(mgx_surface_init(&expected, 31, 29));
    for (unsigned samples = 1; samples <= 8; ++samples)
        for (int evenodd = 0; evenodd <= 1; ++evenodd)
            for (int clipping = 0; clipping <= 1; ++clipping) {
                mgx_rect clip = clipping ? (mgx_rect){3, 2, 20, 19} :
                                           (mgx_rect){-8, -7, 70, 60};
                mgx_clear(&actual, 0x70405060);
                mgx_clear(&expected, 0x70405060);
                actual.clip = expected.clip = clip;
                assert(mulib_glx_raster_fill_checked(&actual, p, 0x916ea243,
                                                     evenodd, samples));
                reference_fill(&expected, p, 0x916ea243, evenodd, samples);
                for (int y = 0; y < actual.h; ++y)
                    for (int x = 0; x < actual.w; ++x)
                        if (actual.pixels[y * actual.stride + x] !=
                            expected.pixels[y * expected.stride + x]) {
                            fprintf(stderr, "raster mismatch at %d,%d: "
                                    "samples=%u evenodd=%d clip=%d\n",
                                    x, y, samples, evenodd, clipping);
                            abort();
                        }
            }
    mgx_surface_free(&actual);
    mgx_surface_free(&expected);
}

static uint32_t random_state = 0x12345678;
static uint32_t random_u32(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void test_svg_bounded_corpus(void)
{
    const char seed[] = "M1.25 2L8 9h3v-2q1 5 8 9c2 3 4 5 6 7z";
    mulib_glx_path_t p = {0};
    assert(mulib_glx_path_move(&p, 3, 4));
    for (size_t length = 0; length < sizeof seed; ++length) {
        int ok = mulib_glx_svg_path_n(&p, seed, length);
        if (!ok)
            assert(p.n == 1 && p.open && p.first.x == 3 && p.first.y == 4);
        mulib_glx_path_free(&p);
        assert(mulib_glx_path_move(&p, 3, 4));
    }
    for (unsigned i = 0; i < 4000; ++i) {
        char data[96];
        size_t length = random_u32() % sizeof data;
        if (i % 2) {
            length = sizeof seed - 1;
            memcpy(data, seed, length);
            data[random_u32() % length] = (char)(random_u32() >> 24);
        } else {
            for (size_t j = 0; j < length; ++j)
                data[j] = (char)(random_u32() >> 24);
        }
        if (!mulib_glx_svg_path_n(&p, data, length))
            assert(p.n == 1 && p.open && p.first.x == 3 && p.first.y == 4);
        else
            assert(mulib_glx_path_valid(&p));
        mulib_glx_path_free(&p);
        assert(mulib_glx_path_move(&p, 3, 4));
    }
    mulib_glx_path_free(&p);
}

static void test_known_coverage_and_order(void)
{
    mulib_glx_path_t p = {0};
    uint32_t pixels[12 * 8];
    mgx_surface s = {0};
    for (size_t i = 0; i < sizeof pixels / sizeof pixels[0]; ++i)
        pixels[i] = 0x12345678;
    assert(mgx_wrap(&s, pixels, 12, 8, 8));
    assert(mulib_glx_svg_path(&p, "M0 0H8V8H0Z M2 2H6V6H2Z"));
    mgx_clear(&s, 0);
    assert(mulib_glx_raster_fill_checked(&s, &p, 0x80ff0000, 0, 4));
    assert(pixels[3 * 12 + 3] == 0x80ff0000); /* Same winding fills the hole. */
    mgx_clear(&s, 0);
    assert(mulib_glx_raster_fill_checked(&s, &p, 0x80ff0000, 1, 4));
    assert(pixels[0] == 0x80ff0000 && pixels[3 * 12 + 3] == 0);
    mulib_glx_path_free(&p);
    assert(mulib_glx_svg_path(&p, "M0 0H8V8H0Z M2 2V6H6V2Z"));
    mgx_clear(&s, 0);
    assert(mulib_glx_raster_fill_checked(&s, &p, 0x80ff0000, 0, 4));
    assert(pixels[0] == 0x80ff0000 && pixels[3 * 12 + 3] == 0);
    mulib_glx_path_free(&p);
    assert(mulib_glx_svg_path(&p, "M0 0H8V8H0Z"));
    mgx_clear(&s, 0);
    assert(mulib_glx_raster_fill_checked(&s, &p, 0x80ff0000, 0, 4));
    assert(mulib_glx_raster_fill_checked(&s, &p, 0x800000ff, 0, 4));
    assert(pixels[0] == mgx_over(0x800000ff, 0x80ff0000));
    assert(pixels[0] != mgx_over(0x80ff0000, 0x800000ff));
    mgx_clear(&s, 0x00123456);
    assert(mulib_glx_raster_fill_checked(&s, &p, 0x00abcdef, 0, 4));
    assert(pixels[0] == mgx_over(0x00abcdef, 0x00123456));
    for (int y = 0; y < 8; ++y)
        for (int x = 8; x < 12; ++x)
            assert(pixels[y * 12 + x] == 0x12345678);
    s.clip = (mgx_rect){INT_MIN, INT_MIN, INT_MAX, INT_MAX};
    assert(mulib_glx_raster_fill_checked(&s, &p, 0xffffffff, 0, 4));
    mulib_glx_path_free(&p);
    assert(mulib_glx_svg_path(&p, "M0 0H.5V1H0Z"));
    assert(mulib_glx_antialias_mask(&p, 0, 0) == 128);
    mulib_glx_path_free(&p);
}

static void test_scanline(void)
{
    static const char *const paths[] = {
        "M1 1H25V25H1Z M6 6H20V20H6Z",
        "M1 1H25V25H1Z M6 6V20H20V6Z",
        "M-5 -8L30 29L-4 29L30 -8Z",
        "M.125 .125L20.125 20.125L.125 20.125L20.125 .125Z",
        "M0 .5H30 M.5 0V29 M2 2L2 2L2 2Z",
        "M-100000000 -100000000H100000000V100000000H-100000000Z",
        "M0 0Q0 29 31 29C5 -10 40 10 15 15Z"
    };
    mulib_glx_path_t p = {0};
    for (size_t i = 0; i < sizeof paths / sizeof paths[0]; ++i) {
        assert(mulib_glx_svg_path(&p, paths[i]));
        compare_fill(&p);
        mulib_glx_path_free(&p);
    }
    for (unsigned trial = 0; trial < 48; ++trial) {
        for (unsigned contour = 0; contour < 3; ++contour)
            for (unsigned i = 0; i < 7; ++i) {
                double x = (double)(random_u32() % 320) / 8 - 5;
                double y = (double)(random_u32() % 320) / 8 - 5;
                assert(mulib_glx_path_add(&p, x, y, i == 0));
            }
        compare_fill(&p); /* Includes self-intersections and implicit closure. */
        mulib_glx_path_free(&p);
    }
    /* A shared triangle edge must not be painted twice at pixel centers. */
    {
        mgx_surface s = {0};
        assert(mgx_surface_init(&s, 8, 8));
        assert(mulib_glx_svg_path(&p, "M0 0L8 0L8 8Z"));
        mulib_glx_raster_fill(&s, &p, 0x80ffffff, 0, 1);
        mulib_glx_path_free(&p);
        assert(mulib_glx_svg_path(&p, "M0 0L8 8L0 8Z"));
        mulib_glx_raster_fill(&s, &p, 0x80ffffff, 0, 1);
        for (int i = 0; i < 64; ++i)
            assert(s.pixels[i] == 0x80ffffff);
        mulib_glx_path_free(&p);
        mgx_surface_free(&s);
    }
}

static void test_failures(void)
{
    mulib_glx_path_t p = {0};
    mgx_surface s = {0};
    assert(mgx_surface_init(&s, 8, 8));
    assert(mulib_glx_svg_path(&p, "M1 1H7V7H1Z"));
    for (long budget = 0; budget < 3; ++budget) {
        mgx_clear(&s, 0x12345678);
        allocation_budget = budget;
        assert(!mulib_glx_raster_fill_checked(&s, &p, 0xffffffff, 0, 4));
        allocation_budget = -1;
        for (int i = 0; i < 64; ++i)
            assert(s.pixels[i] == 0x12345678);
    }
    assert(!mulib_glx_raster_fill_checked(&s, &p, 0xffffffff, 0, 0));
    assert(!mulib_glx_raster_fill_checked(&s, &p, 0xffffffff, 0, 9));
    p.p[1].x = NAN;
    assert(!mulib_glx_raster_fill_checked(&s, &p, 0xffffffff, 0, 4));
    assert(mulib_glx_raster_coverage(&p, 0, 0, 0, 4) == 0);
    p.p[1].x = 7;
    s.stride = 1;
    assert(!mulib_glx_raster_fill_checked(&s, &p, 0xffffffff, 0, 4));
    s.stride = 8;
    mulib_glx_path_free(&p);
    for (long budget = 0; budget < 8; ++budget) {
        int ok;
        assert(mulib_glx_path_move(&p, 2, 3));
        allocation_budget = budget;
        ok = mulib_glx_svg_path(&p, "M0 0C0 500 500 0 500 500Z");
        allocation_budget = -1;
        if (!ok)
            assert(p.n == 1 && p.first.x == 2 && p.first.y == 3 && p.open);
        mulib_glx_path_free(&p);
    }
    for (long budget = 0; budget < 2; ++budget) {
        allocation_budget = budget;
        assert(!mulib_glx_path_move(&p, 1, 1));
        allocation_budget = -1;
        assert(p.n == 0 && !p.open);
        assert(mulib_glx_path_move(&p, 1, 1));
        mulib_glx_path_free(&p);
    }
    mgx_surface_free(&s);
}

int main(void)
{
    test_path_validation();
    test_adaptive_curves();
    test_svg();
    test_svg_bounded_corpus();
    test_scanline();
    test_known_coverage_and_order();
    test_failures();
    puts("gfx path tests passed");
    return 0;
}
