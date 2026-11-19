/* MuLIB graphical extension: input-gamepad. C11, header-only. */
#ifndef MULIB_INPUT_GAMEPAD_H
#define MULIB_INPUT_GAMEPAD_H
#include "mulib-glx-core.h"
typedef struct {
    double axis[16];
    uint64_t buttons;
    uint64_t pressed, released;
} mulib_input_gamepad_t;

static inline int mulib_input_gamepad_axis(mulib_input_gamepad_t *g,
                                          unsigned i, double x, double dead)
{
    if (!g || i >= 16 || !isfinite(x) || !isfinite(dead) || dead < 0 || dead >= 1) {
        errno = EINVAL;
        return 0;
    }
    x = mgx_clamp(x, -1, 1);
    g->axis[i] = fabs(x) <= dead ? 0 : copysign((fabs(x) - dead) / (1 - dead), x);
    return 1;
}

static inline int mulib_input_gamepad_button_checked(mulib_input_gamepad_t *g,
                                                     unsigned i, int on)
{
    uint64_t b;
    if (!g || i >= 64 || (on != 0 && on != 1)) { errno = EINVAL; return 0; }
    b = UINT64_C(1) << i;
    if (on) { g->pressed |= b & ~g->buttons; g->buttons |= b; }
    else { g->released |= b & g->buttons; g->buttons &= ~b; }
    return 1;
}

static inline void mulib_input_gamepad_button(mulib_input_gamepad_t *g, unsigned i, int on)
{
    (void)mulib_input_gamepad_button_checked(g, i, on);
}

static inline void mulib_input_gamepad_frame(mulib_input_gamepad_t *g)
{
    if (g) g->pressed = g->released = 0;
}

static inline void mulib_input_gamepad_cancel(mulib_input_gamepad_t *g)
{
    if (!g) return;
    g->released |= g->buttons;
    g->buttons = 0;
    memset(g->axis, 0, sizeof g->axis);
}
#endif
