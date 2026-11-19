/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_XKB_H
#define MULIB_XKB_H
#include "mulib-wm-core.h"
#include <dlfcn.h>
/* Runtime libxkbcommon adapter, no compile-time dependency. XKB keycodes are evdev + 8. */
typedef struct {
    void *lib, *ctx, *map, *state;
    void *(*context_new)(int);
    void (*context_unref)(void *);
    void *(*keymap_new)(void *, const void *, int);
    void (*keymap_unref)(void *);
    void *(*state_new)(void *);
    void (*state_unref)(void *);
    uint32_t (*update)(void *, uint32_t, int);
    uint32_t (*sym)(void *, uint32_t);
    int (*utf8)(void *, uint32_t, char *, size_t);
} mulib_xkb;

typedef struct {
    const char *rules, *model, *layout, *variant, *options;
} mulib_xkb_names;

static inline int mulib_xkb_load(void *lib, const char *name, void *out, size_t n)
{
    void *p = dlsym(lib, name);
    if (!p || sizeof p != n)
        return 0;
    memcpy(out, &p, n);
    return 1;
}

static inline void mulib_xkb_close(mulib_xkb *x)
{
    if (!x)
        return;
    if (x->state)
        x->state_unref(x->state);
    if (x->map)
        x->keymap_unref(x->map);
    if (x->ctx)
        x->context_unref(x->ctx);
    if (x->lib)
        dlclose(x->lib);
    memset(x, 0, sizeof *x);
}

static inline int mulib_xkb_open(mulib_xkb *out, const mulib_xkb_names *names)
{
    if (!out)
        return 0;
    mulib_xkb x = {0};
    x.lib = dlopen("libxkbcommon.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!x.lib)
        return 0;

#define MULIB_XKB_LOAD(member, symbol) mulib_xkb_load(x.lib, symbol, &x.member, sizeof x.member)
    if (!MULIB_XKB_LOAD(context_new, "xkb_context_new") ||
        !MULIB_XKB_LOAD(context_unref, "xkb_context_unref") ||
        !MULIB_XKB_LOAD(keymap_new, "xkb_keymap_new_from_names") ||
        !MULIB_XKB_LOAD(keymap_unref, "xkb_keymap_unref") ||
        !MULIB_XKB_LOAD(state_new, "xkb_state_new") ||
        !MULIB_XKB_LOAD(state_unref, "xkb_state_unref") ||
        !MULIB_XKB_LOAD(update, "xkb_state_update_key") ||
        !MULIB_XKB_LOAD(sym, "xkb_state_key_get_one_sym") ||
        !MULIB_XKB_LOAD(utf8, "xkb_state_key_get_utf8"))
        goto fail;
#undef MULIB_XKB_LOAD

    x.ctx = x.context_new(0);
    if (!x.ctx)
        goto fail;
    x.map = x.keymap_new(x.ctx, names, 0);
    if (!x.map)
        goto fail;
    x.state = x.state_new(x.map);
    if (!x.state)
        goto fail;
    *out = x;
    return 1;

fail:
    mulib_xkb_close(&x);
    return 0;
}

static inline int mulib_xkb_update(mulib_xkb *x, uint32_t evdev, int down)
{
    if (!x || !x->state || evdev > UINT32_MAX - 8)
        return 0;
    x->update(x->state, evdev + 8, !!down);
    return 1;
}

static inline uint32_t mulib_xkb_sym(mulib_xkb *x, uint32_t evdev)
{
    return x && x->state && evdev <= UINT32_MAX - 8 ? x->sym(x->state, evdev + 8) : 0;
}

static inline int mulib_xkb_utf8(mulib_xkb *x, uint32_t evdev, char *p, size_t n)
{
    return x && x->state && evdev <= UINT32_MAX - 8 && (!n || p)
        ? x->utf8(x->state, evdev + 8, p, n) : -1;
}
#endif
