/* MuLIB graphical extension: glx-atlas. C11, header-only. */
#ifndef MULIB_GLX_ATLAS_H
#define MULIB_GLX_ATLAS_H
#include "mulib-glx-core.h"
typedef struct {int w,h,x,y,row,padding;}mulib_glx_atlas_t;
/* Shelf allocation is transactional, including the output rectangle.
 * Start with positive w/h, nonnegative padding and zero x/y/row. */
static inline int mulib_glx_atlas_alloc(mulib_glx_atlas_t *a,
                                       int w, int h, mgx_rect *out)
{
    int64_t x, y, row, p;
    if (!a || !out || w <= 0 || h <= 0 || a->w <= 0 || a->h <= 0 ||
        a->padding < 0 || a->x < 0 || a->y < 0 || a->row < 0 ||
        a->y > a->h || a->row > a->h - a->y ||
        (int64_t)a->x > (int64_t)a->w + a->padding ||
        w > a->w || h > a->h)
        return 0;
    x = a->x;
    y = a->y;
    row = a->row;
    p = a->padding;
    if (x + w > a->w) {
        x = 0;
        y += row + p;
        row = 0;
    }
    if (y + h > a->h || x + w + p > INT_MAX)
        return 0;
    *out = (mgx_rect){(int)x, (int)y, w, h};
    a->x = (int)(x + w + p);
    a->y = (int)y;
    a->row = mgx_max((int)row, h);
    return 1;
}
#endif
