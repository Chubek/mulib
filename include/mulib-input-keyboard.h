/* MuLIB graphical extension: input-keyboard. C11, header-only. */
#ifndef MULIB_INPUT_KEYBOARD_H
#define MULIB_INPUT_KEYBOARD_H
#include "mulib-glx-core.h"
enum {
    MULIB_INPUT_MOD_SHIFT = 1, MULIB_INPUT_MOD_CTRL = 2,
    MULIB_INPUT_MOD_ALT = 4, MULIB_INPUT_MOD_SUPER = 8
};
typedef struct {
    unsigned char down[768], pressed[768], released[768];
    uint32_t mods;
    unsigned char repeated[768];
} mulib_input_keyboard_t;

static inline uint32_t mulib_input_keyboard_mods(const mulib_input_keyboard_t *k)
{
    if (!k) return 0;
    return (k->down[42] || k->down[54] ? MULIB_INPUT_MOD_SHIFT : 0)
        | (k->down[29] || k->down[97] ? MULIB_INPUT_MOD_CTRL : 0)
        | (k->down[56] || k->down[100] ? MULIB_INPUT_MOD_ALT : 0)
        | (k->down[125] || k->down[126] ? MULIB_INPUT_MOD_SUPER : 0);
}

/* Call once BEFORE consuming a frame's events, never between its edges. */
static inline void mulib_input_keyboard_frame(mulib_input_keyboard_t *k)
{
    if (!k) return;
    memset(k->pressed, 0, sizeof k->pressed);
    memset(k->released, 0, sizeof k->released);
    memset(k->repeated, 0, sizeof k->repeated);
}

static inline int mulib_input_keyboard_update(mulib_input_keyboard_t *k,
                                              unsigned code, int value)
{
    if (!k || code >= 768 || value < 0 || value > 2
        || (value == 2 && !k->down[code])) {
        errno = EINVAL;
        return 0;
    }
    if (value == 2) {
        k->repeated[code] = 1;
        return 1;
    }
    k->pressed[code] |= value == 1 && !k->down[code];
    k->released[code] |= value == 0 && k->down[code];
    k->down[code] = (unsigned char)(value != 0);
    k->mods = mulib_input_keyboard_mods(k);
    return 1;
}

/* Focus/device loss releases held keys without erasing same-frame presses. */
static inline void mulib_input_keyboard_cancel(mulib_input_keyboard_t *k)
{
    if (!k) return;
    for (size_t i = 0; i < 768; ++i) {
        k->released[i] |= k->down[i] != 0;
        k->down[i] = 0;
    }
    k->mods = 0;
}
#endif
