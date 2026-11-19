/* Deliberately small limits exercise resource exhaustion without large inputs. */
#define MULIB_GLX_PATH_MAX_POINTS 32u
#define MULIB_GLX_SVG_MAX_BYTES 128u
#include "mulib-glx-svg.h"
#include <assert.h>

int main(void)
{
    mulib_glx_path_t p = {0};
    assert(mulib_glx_path_move(&p, 1, 2));
    assert(!mulib_glx_path_cubic_tol(&p, (mgx_point){0, 1000},
                                   (mgx_point){1000, 0},
                                   (mgx_point){1000, 1000}, .001));
    assert(p.n == 1 && p.open && p.first.x == 1 && p.first.y == 2);
    assert(!mulib_glx_svg_path(&p, "M0 0C0 1000 1000 0 1000 1000z"));
    assert(p.n == 1 && p.open && p.first.x == 1 && p.first.y == 2);
    assert(!mulib_glx_path_arc(&p, 0, 0, 10000, 0, 6.283185307179586));
    assert(p.n == 1);
    while (p.n < MULIB_GLX_PATH_MAX_POINTS)
        assert(mulib_glx_path_line(&p, 3, 4));
    assert(!mulib_glx_path_line(&p, 5, 6));
    assert(!mulib_glx_path_close(&p));
    assert(p.n == MULIB_GLX_PATH_MAX_POINTS && p.open);
    assert(!mulib_glx_svg_path(&p, "M0 0"));
    assert(p.n == MULIB_GLX_PATH_MAX_POINTS);
    {
        char input[MULIB_GLX_SVG_MAX_BYTES];
        memset(input, ' ', sizeof input);
        assert(mulib_glx_svg_path_n(&p, input, sizeof input));
        assert(!mulib_glx_svg_path(&p, input)); /* Never reads past the cap. */
    }
    mulib_glx_path_free(&p);
    return 0;
}
