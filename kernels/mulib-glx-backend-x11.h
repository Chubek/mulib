/* MuLIB graphical extension: glx-backend-x11. C11, header-only. */
#ifndef MULIB_GLX_BACKEND_X11_H
#define MULIB_GLX_BACKEND_X11_H
#include "mulib-x11-image.h"
#include "mulib-glx-platform.h"
/* Configured drawable/GC only. Caller verifies the server's 32bpp LSBFirst
 * pixmap format and RGB masks; PutImage completion is not presentation.
 * Source pixels have been copied/sent before present returns. */
typedef struct{mx11_connection *connection;uint32_t window,gc;uint8_t depth;}mulib_glx_backend_x11_t;
static inline int mulib_glx_backend_x11_present(void *u,const mgx_surface *s,const mgx_rect *r,size_t n){mulib_glx_backend_x11_t *b=u;(void)r;(void)n;return b&&b->connection&&mulib_x11_image_put(b->connection,b->window,b->gc,s,b->depth);}
#endif
