/* MuLIB graphical extension: gui-dragdrop. C11, header-only. */
#ifndef MULIB_GUI_DRAGDROP_H
#define MULIB_GUI_DRAGDROP_H
#include "mulib-gui-clipboard.h"
typedef struct{mulib_gui_clipboard_t payload;uint64_t source,target;int active;}mulib_gui_dragdrop_t;
static inline int mulib_gui_dragdrop_begin(mulib_gui_dragdrop_t *d,uint64_t source,const char *mime,const void *p,size_t n){if(!d||!source||d->active||!mulib_gui_clipboard_set(&d->payload,mime,p,n))return 0;d->source=source;d->target=0;d->active=1;return 1;}
static inline int mulib_gui_dragdrop_hover(mulib_gui_dragdrop_t *d,uint64_t target,const char *mime){if(!d||!d->active)return 0;d->target=target&&mulib_gui_mime_valid(mime)&&!strcmp(mime,d->payload.mime)?target:0;return d->target!=0;}
static inline int mulib_gui_dragdrop_end(mulib_gui_dragdrop_t *d,uint64_t *target){if(target)*target=0;if(!d||!d->active)return 0;if(target)*target=d->target;d->active=0;return d->target!=0;}
static inline void mulib_gui_dragdrop_free(mulib_gui_dragdrop_t *d){if(d){mulib_gui_clipboard_free(&d->payload);memset(d,0,sizeof *d);}}
#endif
