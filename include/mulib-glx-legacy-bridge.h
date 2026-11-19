#ifndef MULIB_GLX_LEGACY_BRIDGE_H
#define MULIB_GLX_LEGACY_BRIDGE_H
#include "mulib-glx-core.h"
#ifdef MULIB_GFX_ENABLE_LEGACY_GLX
#include "mulib-glx.h"
/* Borrow the original GLX framebuffer; original context outlives the view. */
static inline int mulib_glx_legacy_view(mgx_surface *out,mulib_glx_t *original){if(!original)return 0;size_t pitch=mulib_glx_pitch(original);unsigned w=mulib_glx_width(original),h=mulib_glx_height(original);return pitch%4==0&&w<=INT_MAX&&h<=INT_MAX&&mgx_wrap(out,mulib_glx_pixels(original),pitch/4,(int)w,(int)h);}
#endif
#endif
