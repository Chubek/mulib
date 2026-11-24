/* MuLIB graphical extension: input-gesture. C11, header-only. */
#ifndef MULIB_INPUT_GESTURE_H
#define MULIB_INPUT_GESTURE_H
#include "mulib-input-touch.h"
typedef struct {
    double x, y, distance, angle;
    int tracking;
    int a_slot, b_slot, a_id, b_id;
    uint64_t a_serial, b_serial;
} mulib_input_gesture_t;
typedef struct{double dx,dy,scale,rotation;int active;}mgx_gesture_delta;
typedef mgx_gesture_delta mulib_input_gesture_delta_t;

/* The lowest two active slots define the gesture. Contact replacement resets
 * the baseline, including release+begin between two updates of this helper. */
static inline mgx_gesture_delta mulib_input_gesture_update(mulib_input_gesture_t *g,
                                                          const mulib_input_touch_t *t)
{
    mgx_gesture_delta d = {0, 0, 1, 0, 0};
    const mgx_contact *a, *b;
    int ai = -1, bi = -1;
    double dx, dy, x, y, dist, angle;
    if (!g || !t) { errno = EINVAL; return d; }
    for (int i = 0; i < 32; ++i) {
        if (!t->v[i].active) continue;
        if (ai < 0) ai = i;
        else { bi = i; break; }
    }
    if (bi < 0) { g->tracking = 0; return d; }
    a = t->v + ai; b = t->v + bi;
    if (!isfinite(a->x) || !isfinite(a->y) || !isfinite(b->x) || !isfinite(b->y))
        goto invalid;
    x = a->x / 2 + b->x / 2; y = a->y / 2 + b->y / 2;
    dx = b->x - a->x; dy = b->y - a->y;
    dist = hypot(dx, dy); angle = atan2(dy, dx);
    if (!isfinite(dx) || !isfinite(dy) || !isfinite(dist) || !isfinite(angle))
        goto invalid;
    if (g->tracking && g->distance > 0 && g->a_slot == ai && g->b_slot == bi
        && g->a_id == a->id && g->b_id == b->id
        && g->a_serial == t->serial[ai] && g->b_serial == t->serial[bi]) {
        double mx = x - g->x, my = y - g->y, scale = dist / g->distance;
        double rotation = remainder(angle - g->angle, 6.28318530717958647693);
        if (!isfinite(mx) || !isfinite(my) || !isfinite(scale) || !isfinite(rotation))
            goto invalid;
        d = (mgx_gesture_delta){mx, my, scale, rotation, 1};
    }
    g->x = x; g->y = y; g->distance = dist; g->angle = angle; g->tracking = 1;
    g->a_slot = ai; g->b_slot = bi; g->a_id = a->id; g->b_id = b->id;
    g->a_serial = t->serial[ai]; g->b_serial = t->serial[bi];
    return d;
invalid:
    g->tracking = 0;
    errno = ERANGE;
    return d;
}
#endif
