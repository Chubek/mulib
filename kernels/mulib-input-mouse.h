/* MuLIB graphical extension: input-mouse. C11, header-only. */
#ifndef MULIB_INPUT_MOUSE_H
#define MULIB_INPUT_MOUSE_H
#include "mulib-glx-core.h"
typedef struct{double x,y,scroll_x,scroll_y;unsigned buttons,pressed,released;}mulib_input_mouse_t;
static inline int mulib_input_mouse_motion_checked(mulib_input_mouse_t *m,
    double dx, double dy, int w, int h, double speed)
{
    double x, y;
    if (!m || w <= 0 || h <= 0 || !isfinite(dx) || !isfinite(dy)
        || !isfinite(speed) || speed < 0 || !isfinite(m->x) || !isfinite(m->y)) {
        errno = EINVAL;
        return 0;
    }
    x = m->x + dx * speed;
    y = m->y + dy * speed;
    if (!isfinite(x) || !isfinite(y)) { errno = ERANGE; return 0; }
    m->x = mgx_clamp(x, 0, w - 1);
    m->y = mgx_clamp(y, 0, h - 1);
    return 1;
}

static inline void mulib_input_mouse_motion(mulib_input_mouse_t *m,
    double dx, double dy, int w, int h, double speed)
{
    (void)mulib_input_mouse_motion_checked(m, dx, dy, w, h, speed);
}

static inline int mulib_input_mouse_button_checked(mulib_input_mouse_t *m,
                                                   unsigned n, int down)
{
    unsigned b;
    if (!m || n >= 32 || n >= sizeof(unsigned) * CHAR_BIT
        || (down != 0 && down != 1)) { errno = EINVAL; return 0; }
    b = 1u << n;
    if (down) { m->pressed |= b & ~m->buttons; m->buttons |= b; }
    else { m->released |= b & m->buttons; m->buttons &= ~b; }
    return 1;
}

static inline void mulib_input_mouse_button(mulib_input_mouse_t *m, unsigned n, int down)
{
    (void)mulib_input_mouse_button_checked(m, n, down);
}

static inline int mulib_input_mouse_scroll(mulib_input_mouse_t *m, double x, double y)
{
    double sx, sy;
    if (!m || !isfinite(x) || !isfinite(y)
        || !isfinite(m->scroll_x) || !isfinite(m->scroll_y)) {
        errno = EINVAL;
        return 0;
    }
    sx = m->scroll_x + x; sy = m->scroll_y + y;
    if (!isfinite(sx) || !isfinite(sy)) { errno = ERANGE; return 0; }
    m->scroll_x = sx; m->scroll_y = sy;
    return 1;
}

static inline void mulib_input_mouse_frame(mulib_input_mouse_t *m)
{
    if (m) { m->pressed = m->released = 0; m->scroll_x = m->scroll_y = 0; }
}

static inline void mulib_input_mouse_cancel(mulib_input_mouse_t *m)
{
    if (m) { m->released |= m->buttons; m->buttons = 0; }
}
#endif
