/* MuLIB graphical extension: glx-path. C11, header-only. */
#ifndef MULIB_GLX_PATH_H
#define MULIB_GLX_PATH_H
#include "mulib-glx-core.h"

/* Set consistently in all translation units before including this header. */
#ifndef MULIB_GLX_PATH_MAX_POINTS
#define MULIB_GLX_PATH_MAX_POINTS 1048576u
#endif
#define MULIB_GLX_PATH_COORD_LIMIT 100000000.0
#define MULIB_GLX_PATH_FLATNESS 0.25

/* Owning object: zero-initialize, do not copy, and release with path_free. */
typedef struct {
    mgx_point *p;
    unsigned char *move;
    size_t n, cap, mc;
    mgx_point first;
    int open;
} mulib_glx_path_t;

static inline int mulib_glx_path_point_valid(mgx_point p)
{
    return isfinite(p.x) && isfinite(p.y) &&
           fabs(p.x) <= MULIB_GLX_PATH_COORD_LIMIT &&
           fabs(p.y) <= MULIB_GLX_PATH_COORD_LIMIT;
}

static inline int mulib_glx_path_storage_valid(const mulib_glx_path_t *p)
{
    return p && p->n <= MULIB_GLX_PATH_MAX_POINTS && p->n <= INT_MAX &&
           p->n <= p->cap && p->n <= p->mc &&
           (!p->cap || p->p) && (!p->mc || p->move);
}

/* Read-only consumers also accept borrowed arrays (capacities are ignored). */
static inline int mulib_glx_path_valid(const mulib_glx_path_t *p)
{
    if (!p || p->n > MULIB_GLX_PATH_MAX_POINTS || p->n > INT_MAX ||
        (p->n && (!p->p || !p->move)))
        return 0;
    for (size_t i = 0; i < p->n; ++i)
        if (!mulib_glx_path_point_valid(p->p[i]))
            return 0;
    return 1;
}

static inline int mulib_glx_path_add(mulib_glx_path_t *p,
                                    double x, double y, int move)
{
    mgx_point point = {x, y};
    if (!mulib_glx_path_storage_valid(p) ||
        p->n >= MULIB_GLX_PATH_MAX_POINTS || p->n >= INT_MAX ||
        !mulib_glx_path_point_valid(point) ||
        !mgx_grow((void **)&p->p, &p->cap, p->n + 1, sizeof *p->p) ||
        !mgx_grow((void **)&p->move, &p->mc, p->n + 1, sizeof *p->move))
        return 0;
    if (move || !p->open) {
        p->first = point;
        p->open = 1;
        move = 1;
    }
    p->p[p->n] = point;
    p->move[p->n++] = (unsigned char)(move != 0);
    return 1;
}

static inline int mulib_glx_path_move(mulib_glx_path_t *p, double x, double y)
{
    return mulib_glx_path_add(p, x, y, 1);
}

static inline int mulib_glx_path_line(mulib_glx_path_t *p, double x, double y)
{
    return mulib_glx_path_add(p, x, y, 0);
}

static inline int mulib_glx_path_close(mulib_glx_path_t *p)
{
    if (!mulib_glx_path_storage_valid(p) || !p->open || !p->n)
        return 0;
    if (!mulib_glx_path_line(p, p->first.x, p->first.y))
        return 0;
    p->open = 0;
    return 1;
}

typedef struct {
    mgx_point a, b, c, d;
    unsigned depth;
} mulib_glx_path_curve_t;

static inline mgx_point mulib_glx_path_midpoint(mgx_point a, mgx_point b)
{
    return (mgx_point){(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
}

/* Adaptive de Casteljau subdivision. The control polygon must be within
 * tolerance of the finite chord, not merely its infinite line: this retains
 * collinear reversals and loops. Failure restores the logical path, although
 * any successfully grown allocation remains owned by it. Depth is bounded. */
static inline int mulib_glx_path_cubic_tol(mulib_glx_path_t *p, mgx_point b,
                                          mgx_point c, mgx_point d,
                                          double tolerance)
{
    enum { max_depth = 24 };
    mulib_glx_path_curve_t stack[max_depth + 2];
    size_t count = 0, saved;
    if (!mulib_glx_path_storage_valid(p) || !p->open || !p->n ||
        !isfinite(tolerance) || tolerance <= 0 ||
        !mulib_glx_path_point_valid(p->p[p->n - 1]) ||
        !mulib_glx_path_point_valid(b) ||
        !mulib_glx_path_point_valid(c) || !mulib_glx_path_point_valid(d))
        return 0;
    saved = p->n;
    stack[count++] = (mulib_glx_path_curve_t){p->p[p->n - 1], b, c, d, 0};
    while (count) {
        mulib_glx_path_curve_t v = stack[--count];
        if (mgx_distseg(v.b.x, v.b.y, v.a, v.d) <= tolerance &&
            mgx_distseg(v.c.x, v.c.y, v.a, v.d) <= tolerance) {
            if (!mulib_glx_path_line(p, v.d.x, v.d.y))
                goto fail;
        } else {
            mgx_point ab, bc, cd, abc, bcd, middle;
            if (v.depth == max_depth)
                goto fail;
            ab = mulib_glx_path_midpoint(v.a, v.b);
            bc = mulib_glx_path_midpoint(v.b, v.c);
            cd = mulib_glx_path_midpoint(v.c, v.d);
            abc = mulib_glx_path_midpoint(ab, bc);
            bcd = mulib_glx_path_midpoint(bc, cd);
            middle = mulib_glx_path_midpoint(abc, bcd);
            stack[count++] = (mulib_glx_path_curve_t){
                middle, bcd, cd, v.d, v.depth + 1};
            stack[count++] = (mulib_glx_path_curve_t){
                v.a, ab, abc, middle, v.depth + 1};
        }
    }
    return 1;
fail:
    p->n = saved;
    return 0;
}

static inline int mulib_glx_path_cubic(mulib_glx_path_t *p, mgx_point b,
                                      mgx_point c, mgx_point d)
{
    return mulib_glx_path_cubic_tol(p, b, c, d, MULIB_GLX_PATH_FLATNESS);
}

static inline int mulib_glx_path_quad_tol(mulib_glx_path_t *p, mgx_point b,
                                         mgx_point c, double tolerance)
{
    mgx_point a;
    if (!mulib_glx_path_storage_valid(p) || !p->n || !p->open ||
        !mulib_glx_path_point_valid(b) || !mulib_glx_path_point_valid(c))
        return 0;
    a = p->p[p->n - 1];
    return mulib_glx_path_cubic_tol(
        p, (mgx_point){a.x + 2 * (b.x - a.x) / 3,
                       a.y + 2 * (b.y - a.y) / 3},
        (mgx_point){c.x + 2 * (b.x - c.x) / 3,
                    c.y + 2 * (b.y - c.y) / 3}, c, tolerance);
}

static inline int mulib_glx_path_quad(mulib_glx_path_t *p,
                                     mgx_point b, mgx_point c)
{
    return mulib_glx_path_quad_tol(p, b, c, MULIB_GLX_PATH_FLATNESS);
}

/* Circular center/radius arc, angles in radians. Not SVG elliptical arc data.
 * Includes both endpoints, connecting the first to the current open contour. */
static inline int mulib_glx_path_arc(mulib_glx_path_t *p, double x, double y,
                                    double r, double a, double b)
{
    double delta, step, steps;
    size_t n, saved;
    mgx_point first;
    int open;
    if (!mulib_glx_path_storage_valid(p) ||
        !mulib_glx_path_point_valid((mgx_point){x, y}) ||
        !isfinite(r) || r <= 0 || !isfinite(a) || !isfinite(b))
        return 0;
    delta = b - a;
    if (!isfinite(delta))
        return 0;
    /* Stable sagitta bound: r * (1 - cos(step / 2)) <= flatness. */
    step = fmin(1.57079632679489661923,
                4 * asin(sqrt(fmin(1, MULIB_GLX_PATH_FLATNESS / r / 2))));
    if (step <= 0)
        return 0;
    steps = fmax(1, ceil(fabs(delta) / step));
    if (!isfinite(steps) || steps >= MULIB_GLX_PATH_MAX_POINTS ||
        steps >= INT_MAX)
        return 0;
    n = (size_t)steps;
    if (n + 1 > MULIB_GLX_PATH_MAX_POINTS - p->n ||
        n + 1 > (size_t)INT_MAX - p->n)
        return 0;
    saved = p->n;
    first = p->first;
    open = p->open;
    for (size_t i = 0; i <= n; ++i) {
        double angle = i == n ? b : a + delta * ((double)i / (double)n);
        if (!mulib_glx_path_line(p, x + r * cos(angle), y + r * sin(angle))) {
            p->n = saved;
            p->first = first;
            p->open = open;
            return 0;
        }
    }
    return 1;
}

static inline void mulib_glx_path_free(mulib_glx_path_t *p)
{
    if (p) {
        free(p->p);
        free(p->move);
        memset(p, 0, sizeof *p);
    }
}
#endif
