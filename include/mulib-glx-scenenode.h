/* MuLIB notebook desktop extension. C11, header-only. SPDX-License-Identifier: MIT */
#ifndef MULIB_GLX_SCENENODE_H
#define MULIB_GLX_SCENENODE_H
#include "mulib-glx-clientbuffer.h"

typedef struct {
    uint64_t id, parent;
    double x, y, sx, sy, opacity;
    mulib_wm_rect clip;
    int clipped, visible, accept_input;
    mulib_glx_clientbuffer *buffer;
} mulib_glx_scenenode;

static inline mulib_glx_scenenode mulib_glx_scenenode_default(void)
{
    return (mulib_glx_scenenode){
        .sx = 1, .sy = 1, .opacity = 1, .visible = 1, .accept_input = 1};
}

static inline int mulib_glx_scenenode_valid(const mulib_glx_scenenode *n)
{
    return n && isfinite(n->x) && isfinite(n->y) && isfinite(n->sx) &&
           isfinite(n->sy) && n->sx > 0 && n->sy > 0 &&
           isfinite(n->opacity) && n->opacity >= 0 && n->opacity <= 1 &&
           (!n->clipped || mulib_wm_rect_valid(n->clip)) &&
           (!n->buffer || mulib_glx_clientbuffer_valid(n->buffer));
}

/* n is caller-owned or borrowed from an idle scene, never a traversal callback.
 * Replacement is published before release; release must not mutate/free n. */
static inline int mulib_glx_scenenode_buffer(mulib_glx_scenenode *n,
                                            mulib_glx_clientbuffer *b)
{
    if (!n || (b && !mulib_glx_clientbuffer_ref(b)))
        return 0;
    mulib_glx_clientbuffer *old = n->buffer;
    n->buffer = b;
    mulib_glx_clientbuffer_unref(old);
    return 1;
}
#endif
