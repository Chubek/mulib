/* MuLIB graphical extension: input-keymap. C11, header-only. */
#ifndef MULIB_INPUT_KEYMAP_H
#define MULIB_INPUT_KEYMAP_H
#include "mulib-input-keyboard.h"
typedef struct{uint32_t normal[768],shift[768];}mulib_input_keymap_t;
/* Fixed evdev US mapping, not XKB. KEY_GRAVE is 41, NOT KEY_LEFTSHIFT (42). */
static inline void mulib_input_keymap_us(mulib_input_keymap_t *m)
{
    const char *rows[] = {"1234567890-=", "qwertyuiop[]", "asdfghjkl;'", "zxcvbnm,./"};
    const char *up[] = {"!@#$%^&*()_+", "QWERTYUIOP{}", "ASDFGHJKL:\"", "ZXCVBNM<>?"};
    const unsigned base[] = {2, 16, 30, 44};
    if (!m) return;
    memset(m, 0, sizeof *m);
    for (size_t k = 0; k < 4; ++k)
        for (size_t i = 0; rows[k][i]; ++i) {
            m->normal[base[k] + i] = (unsigned char)rows[k][i];
            m->shift[base[k] + i] = (unsigned char)up[k][i];
        }
    m->normal[41] = '`'; m->shift[41] = '~';
    m->normal[43] = '\\'; m->shift[43] = '|';
    m->normal[57] = m->shift[57] = ' ';
    m->normal[28] = m->shift[28] = '\n';
    m->normal[15] = m->shift[15] = '\t';
}

static inline uint32_t mulib_input_keymap_translate(const mulib_input_keymap_t *m,
                                                   unsigned code, unsigned mods,
                                                   int caps)
{
    uint32_t v;
    int shift;
    if (!m || code >= 768 || (mods & ~MULIB_INPUT_MOD_SHIFT)) return 0;
    v = m->normal[code];
    shift = (mods & MULIB_INPUT_MOD_SHIFT) != 0;
    if (v >= 'a' && v <= 'z') shift ^= !!caps;
    v = shift ? m->shift[code] : v;
    return v <= 0x10ffffu && !(v >= 0xd800u && v <= 0xdfffu) ? v : 0;
}
#endif
