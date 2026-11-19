/* MuLIB graphical extension: input-tablet. C11, header-only. */
#ifndef MULIB_INPUT_TABLET_H
#define MULIB_INPUT_TABLET_H
#include "mulib-glx-core.h"
typedef struct{double x,y,pressure,tilt_x,tilt_y;int proximity,tip;}mulib_input_tablet_t;
static inline int mulib_input_tablet_axis_checked(int value, int minimum,
                                                 int maximum, double *out)
{
    if (!out || maximum <= minimum) { errno = EINVAL; return 0; }
    *out = mgx_clamp(((double)value - minimum) / ((double)maximum - minimum), 0, 1);
    return 1;
}

/* Legacy sentinel: invalid ranges return 0 with errno == EINVAL. */
static inline double mulib_input_tablet_axis(int value, int minimum, int maximum)
{
    double out = 0;
    (void)mulib_input_tablet_axis_checked(value, minimum, maximum, &out);
    return out;
}

static inline int mulib_input_tablet_pressure_checked(mulib_input_tablet_t *t,
                                                     int value, int min, int max)
{
    double pressure;
    if (!t) { errno = EINVAL; return 0; }
    if (!mulib_input_tablet_axis_checked(value, min, max, &pressure)) return 0;
    t->pressure = t->proximity ? pressure : 0;
    t->tip = t->proximity && t->pressure > 0;
    return 1;
}

static inline void mulib_input_tablet_pressure(mulib_input_tablet_t *t,
                                              int value, int min, int max)
{
    (void)mulib_input_tablet_pressure_checked(t, value, min, max);
}

static inline int mulib_input_tablet_proximity(mulib_input_tablet_t *t, int in)
{
    if (!t || (in != 0 && in != 1)) { errno = EINVAL; return 0; }
    t->proximity = in;
    if (!in) { t->pressure = 0; t->tip = 0; }
    return 1;
}

/* Positions are surface coordinates; normalized tilts clamp to [-1,1]. */
static inline int mulib_input_tablet_motion(mulib_input_tablet_t *t,
                                           double x, double y, double tx, double ty)
{
    if (!t || !isfinite(x) || !isfinite(y) || !isfinite(tx) || !isfinite(ty)) {
        errno = EINVAL;
        return 0;
    }
    t->x = x; t->y = y;
    t->tilt_x = mgx_clamp(tx, -1, 1); t->tilt_y = mgx_clamp(ty, -1, 1);
    return 1;
}
#endif
