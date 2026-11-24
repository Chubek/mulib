/* MuLIB graphical extension: immgtk-dnd. C11, header-only. */
#ifndef MULIB_IMMGTK_DND_H
#define MULIB_IMMGTK_DND_H
#include "mulib-immgtk-native.h"
#include "mulib-gui-dragdrop.h"
static inline int mulib_immgtk_dnd_target(mulib_immgtk_native_t *g,mulib_gui_dragdrop_t *d,const char *id,mgx_rect r,const char *mime){if(!d->active||!mgx_contains(r,g->x,g->y)||!mgx_contains(g->clip,g->x,g->y))return 0;return mulib_gui_dragdrop_hover(d,mig_id(g,id),mime);}
#endif
