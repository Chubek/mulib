/* MuLIB graphical extension: immgtk-nav. C11, header-only. */
#ifndef MULIB_IMMGTK_NAV_H
#define MULIB_IMMGTK_NAV_H
#include "mulib-immgtk-native.h"
static inline uint64_t mulib_immgtk_nav_step(mulib_immgtk_native_t *g, int reverse) {
    return mig_nav_step(g, reverse);
}
#endif
