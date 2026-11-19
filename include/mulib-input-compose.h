/* MuLIB graphical extension: input-compose. C11, header-only. */
#ifndef MULIB_INPUT_COMPOSE_H
#define MULIB_INPUT_COMPOSE_H
#include "mulib-glx-core.h"
typedef struct{uint32_t dead;}mulib_input_compose_t;
static inline int mulib_input_compose_dead(uint32_t key)
{
    return key == 0x301 || key == 0x300 || key == 0x308 || key == 0x303;
}

/* Limited Latin dead-key helper, not a Unicode normalizer or IME.
 * Returns 0 while composing, 1/2 scalars on completion, SIZE_MAX on invalid
 * input (state/output unchanged). Unknown pairs emit base then combining mark. */
static inline size_t mulib_input_compose_feed(mulib_input_compose_t *c,
                                             uint32_t key, uint32_t out[2])
{
    static const struct { uint32_t dead, base, result; } pairs[] = {
        {0x301,'a',225}, {0x301,'e',233}, {0x301,'i',237}, {0x301,'o',243},
        {0x301,'u',250}, {0x301,'y',253}, {0x301,'A',193}, {0x301,'E',201},
        {0x301,'I',205}, {0x301,'O',211}, {0x301,'U',218}, {0x301,'Y',221},
        {0x300,'a',224}, {0x300,'e',232}, {0x300,'i',236}, {0x300,'o',242},
        {0x300,'u',249}, {0x300,'A',192}, {0x300,'E',200}, {0x300,'I',204},
        {0x300,'O',210}, {0x300,'U',217},
        {0x308,'a',228}, {0x308,'e',235}, {0x308,'i',239}, {0x308,'o',246},
        {0x308,'u',252}, {0x308,'y',255}, {0x308,'A',196}, {0x308,'E',203},
        {0x308,'I',207}, {0x308,'O',214}, {0x308,'U',220}, {0x308,'Y',376},
        {0x303,'a',227}, {0x303,'n',241}, {0x303,'o',245},
        {0x303,'A',195}, {0x303,'N',209}, {0x303,'O',213}
    };
    uint32_t dead;
    if (!c || !out || key > 0x10ffffu || (key >= 0xd800u && key <= 0xdfffu)
        || (c->dead && !mulib_input_compose_dead(c->dead))) {
        errno = EINVAL;
        return SIZE_MAX;
    }
    if (!c->dead) {
        if (mulib_input_compose_dead(key)) { c->dead = key; return 0; }
        out[0] = key;
        return 1;
    }
    dead = c->dead;
    c->dead = 0;
    if (key == ' ' || key == dead) {
        out[0] = dead == 0x301 ? 0xb4 : dead == 0x300 ? '`'
               : dead == 0x308 ? 0xa8 : '~';
        return 1;
    }
    for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; ++i) {
        if (pairs[i].dead == dead && pairs[i].base == key) {
            out[0] = pairs[i].result;
            return 1;
        }
    }
    out[0] = key; out[1] = dead;
    return 2;
}

static inline void mulib_input_compose_cancel(mulib_input_compose_t *c)
{
    if (c) c->dead = 0;
}
#endif
