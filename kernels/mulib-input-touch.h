/* MuLIB graphical extension: input-touch. C11, header-only. */
#ifndef MULIB_INPUT_TOUCH_H
#define MULIB_INPUT_TOUCH_H
#include "mulib-glx-core.h"
typedef struct{int id,active;double x,y,pressure;}mgx_contact;
typedef mgx_contact mulib_input_contact_t;
typedef struct {
    mgx_contact v[32];
    uint32_t pressed, released;
    uint64_t serial[32];
} mulib_input_touch_t;

/* id == -1 ends a contact. Active IDs must be unique. Coordinates are in the
 * caller's surface space; pressure is clamped to [0,1]. Invalid input is atomic. */
static inline int mulib_input_touch_update(mulib_input_touch_t *t, int slot, int id,
                                           double x, double y, double pressure)
{
    uint32_t bit;
    int replacing;
    if (!t || slot < 0 || slot >= 32 || id < -1
        || !isfinite(x) || !isfinite(y) || !isfinite(pressure)) {
        errno = EINVAL;
        return 0;
    }
    for (int i = 0; i < 32; ++i) {
        if (i != slot && id >= 0 && t->v[i].active && t->v[i].id == id) {
            errno = EEXIST;
            return 0;
        }
    }
    bit = UINT32_C(1) << slot;
    replacing = t->v[slot].active && t->v[slot].id != id;
    if (replacing) t->released |= bit;
    if (id >= 0 && (!t->v[slot].active || replacing)) {
        t->pressed |= bit;
        t->serial[slot] = t->serial[slot] == UINT64_MAX ? 1 : t->serial[slot] + 1;
    }
    t->v[slot] = (mgx_contact){id, id >= 0, x, y,
                             id >= 0 ? mgx_clamp(pressure, 0, 1) : 0};
    return 1;
}

static inline size_t mulib_input_touch_count(const mulib_input_touch_t *t)
{
    size_t n = 0;
    if (t) for (int i = 0; i < 32; ++i) n += t->v[i].active != 0;
    return n;
}

static inline void mulib_input_touch_frame(mulib_input_touch_t *t)
{
    if (t) t->pressed = t->released = 0;
}

static inline void mulib_input_touch_cancel(mulib_input_touch_t *t)
{
    if (!t) return;
    for (int i = 0; i < 32; ++i) {
        if (t->v[i].active) t->released |= UINT32_C(1) << i;
        t->v[i].active = 0;
        t->v[i].id = -1;
        t->v[i].pressure = 0;
    }
}
#endif
