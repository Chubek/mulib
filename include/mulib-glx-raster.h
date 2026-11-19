/* MuLIB graphical extension: glx-raster. C11, header-only. */
#ifndef MULIB_GLX_RASTER_H
#define MULIB_GLX_RASTER_H
#include "mulib-glx-path.h"

/* Scalar reference and point queries; fill below does not call this per pixel.
 * All contours are implicitly closed. Crossings use [ymin, ymax). */
static inline int mulib_glx_raster_winding_unchecked(
    const mulib_glx_path_t *p, double x, double y)
{
    int winding = 0;
    size_t start = 0;
    while (start < p->n) {
        size_t end = start + 1;
        while (end < p->n && !p->move[end])
            ++end;
        for (size_t i = start; i < end; ++i) {
            mgx_point a = p->p[i], b = p->p[i + 1 < end ? i + 1 : start];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                double cross = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (cross > x)
                    winding += b.y > a.y ? 1 : -1;
            }
        }
        start = end;
    }
    return winding;
}

static inline int mulib_glx_raster_winding(const mulib_glx_path_t *p,
                                          double x, double y)
{
    if (!mulib_glx_path_valid(p) || !isfinite(x) || !isfinite(y))
        return 0;
    return mulib_glx_raster_winding_unchecked(p, x, y);
}

static inline double mulib_glx_raster_coverage(const mulib_glx_path_t *p,
                                              int x, int y, int evenodd,
                                              unsigned samples)
{
    unsigned n = 0;
    if (!mulib_glx_path_valid(p) || samples < 1 || samples > 8)
        return 0;
    for (unsigned j = 0; j < samples; ++j)
        for (unsigned i = 0; i < samples; ++i) {
            int w = mulib_glx_raster_winding_unchecked(
                p, x + (i + .5) / samples, y + (j + .5) / samples);
            n += evenodd ? (w % 2 != 0) : (w != 0);
        }
    return (double)n / (samples * samples);
}

typedef struct {
    mgx_point a, b;
    double ymin, ymax, x;
    int winding;
} mulib_glx_raster_edge_t;

static inline int mulib_glx_raster_edge_start_compare(const void *a,
                                                     const void *b)
{
    const mulib_glx_raster_edge_t *ea = (const mulib_glx_raster_edge_t *)a;
    const mulib_glx_raster_edge_t *eb = (const mulib_glx_raster_edge_t *)b;
    return (ea->ymin > eb->ymin) - (ea->ymin < eb->ymin);
}

static inline int mulib_glx_raster_edge_x_compare(const void *a, const void *b)
{
    const mulib_glx_raster_edge_t *ea =
        *(const mulib_glx_raster_edge_t *const *)a;
    const mulib_glx_raster_edge_t *eb =
        *(const mulib_glx_raster_edge_t *const *)b;
    return (ea->x > eb->x) - (ea->x < eb->x);
}

/* Convert a crossing to the first sample >= x. Correct the ceil estimate using
 * precisely the same pixel-local sample expression as the reference, including
 * non-power-of-two grids. Clipped sample indices are always nonnegative. */
static inline double mulib_glx_raster_sample_x(int64_t k, unsigned samples)
{
    return (double)(k / samples) + ((double)(k % samples) + .5) / samples;
}

static inline int64_t mulib_glx_raster_sample_bound(
    double x, int64_t lo, int64_t hi, unsigned samples)
{
    double estimate = ceil(x * samples - .5);
    int64_t k = estimate <= (double)lo ? lo :
                estimate >= (double)hi ? hi : (int64_t)estimate;
    while (k > lo && mulib_glx_raster_sample_x(k - 1, samples) >= x)
        --k;
    while (k < hi && mulib_glx_raster_sample_x(k, samples) < x)
        ++k;
    return k;
}

static inline void mulib_glx_raster_span(unsigned char *row, mgx_rect r,
                                        double left, double right,
                                        unsigned samples)
{
    int64_t lo = (int64_t)r.x * samples;
    int64_t hi = ((int64_t)r.x + r.w) * samples;
    int64_t a = mulib_glx_raster_sample_bound(left, lo, hi, samples);
    int64_t b = mulib_glx_raster_sample_bound(right, a, hi, samples);
    int64_t first, last;
    if (a >= b)
        return;
    first = a / samples;
    last = (b - 1) / samples;
    if (first == last) {
        row[(size_t)(first - r.x)] += (unsigned char)(b - a);
        return;
    }
    row[(size_t)(first - r.x)] += (unsigned char)(samples - a % samples);
    for (int64_t x = first + 1; x < last; ++x)
        row[(size_t)(x - r.x)] += (unsigned char)samples;
    row[(size_t)(last - r.x)] += (unsigned char)((b - 1) % samples + 1);
}

/* Active-edge scan conversion with a samples x samples grid (1..8).
 * O(edges + clipped width) scratch space, never an edge walk per pixel.
 * Coverage is accumulated before source-over, so translucent contours and
 * self-intersections are composited once, in the caller's painter order.
 * Returns 0 on invalid input/allocation failure, without painting anything. */
static inline int mulib_glx_raster_fill_checked(
    mgx_surface *s, const mulib_glx_path_t *p, uint32_t color,
    int evenodd, unsigned samples)
{
    mulib_glx_raster_edge_t *edges = NULL, **active = NULL;
    unsigned char *row = NULL;
    size_t ne = 0, na = 0, next = 0, start = 0;
    double x0, x1, y0, y1;
    mgx_rect r;
    if (!s || !s->pixels || s->w <= 0 || s->h <= 0 ||
        s->stride < (size_t)s->w ||
        s->stride > SIZE_MAX / (size_t)s->h / sizeof *s->pixels ||
        !mulib_glx_path_valid(p) || samples < 1 || samples > 8)
        return 0;
    if (p->n < 3)
        return 1;
    x0 = x1 = p->p[0].x;
    y0 = y1 = p->p[0].y;
    for (size_t i = 1; i < p->n; ++i) {
        x0 = fmin(x0, p->p[i].x);
        x1 = fmax(x1, p->p[i].x);
        y0 = fmin(y0, p->p[i].y);
        y1 = fmax(y1, p->p[i].y);
    }
    r = mgx_intersect(
        mgx_intersect(s->clip, (mgx_rect){0, 0, s->w, s->h}),
        (mgx_rect){(int)floor(x0), (int)floor(y0),
                   (int)(ceil(x1) - floor(x0)), (int)(ceil(y1) - floor(y0))});
    if (!r.w || !r.h)
        return 1;
    if (p->n > SIZE_MAX / sizeof *edges || p->n > SIZE_MAX / sizeof *active)
        return 0;
    edges = (mulib_glx_raster_edge_t *)malloc(p->n * sizeof *edges);
    active = (mulib_glx_raster_edge_t **)malloc(p->n * sizeof *active);
    row = (unsigned char *)malloc((size_t)r.w);
    if (!edges || !active || !row) {
        free(edges);
        free(active);
        free(row);
        return 0;
    }
    while (start < p->n) {
        size_t end = start + 1;
        while (end < p->n && !p->move[end])
            ++end;
        for (size_t i = start; i < end; ++i) {
            mgx_point a = p->p[i], b = p->p[i + 1 < end ? i + 1 : start];
            if (a.y != b.y)
                edges[ne++] = (mulib_glx_raster_edge_t){
                    a, b, fmin(a.y, b.y), fmax(a.y, b.y), 0,
                    b.y > a.y ? 1 : -1};
        }
        start = end;
    }
    qsort(edges, ne, sizeof *edges, mulib_glx_raster_edge_start_compare);
    for (int y = r.y; y < r.y + r.h; ++y) {
        memset(row, 0, (size_t)r.w);
        for (unsigned j = 0; j < samples; ++j) {
            double sy = y + (j + .5) / samples;
            size_t kept = 0;
            int winding = 0;
            double left = 0;
            for (size_t i = 0; i < na; ++i)
                if (active[i]->ymax > sy)
                    active[kept++] = active[i];
            na = kept;
            while (next < ne && edges[next].ymin <= sy) {
                if (edges[next].ymax > sy)
                    active[na++] = &edges[next];
                ++next;
            }
            for (size_t i = 0; i < na; ++i) {
                mulib_glx_raster_edge_t *e = active[i];
                e->x = e->a.x + (sy - e->a.y) *
                       (e->b.x - e->a.x) / (e->b.y - e->a.y);
            }
            qsort(active, na, sizeof *active, mulib_glx_raster_edge_x_compare);
            for (size_t i = 0; i < na;) {
                double right = active[i]->x;
                int delta = 0;
                if (evenodd ? winding % 2 != 0 : winding != 0)
                    mulib_glx_raster_span(row, r, left, right, samples);
                do {
                    delta += active[i++]->winding;
                } while (i < na && active[i]->x == right);
                winding -= delta;
                left = right;
            }
        }
        for (int x = 0; x < r.w; ++x)
            if (row[x])
                mgx_pixel(s, r.x + x, y,
                          mgx_alpha(color, (double)row[x] / (samples * samples)));
    }
    free(row);
    free(active);
    free(edges);
    return 1;
}

/* Source-compatible void wrapper; use fill_checked to observe failures. */
static inline void mulib_glx_raster_fill(mgx_surface *s,
                                        const mulib_glx_path_t *p,
                                        uint32_t color, int evenodd,
                                        unsigned samples)
{
    (void)mulib_glx_raster_fill_checked(s, p, color, evenodd, samples);
}
#endif
