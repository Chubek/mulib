/* MuLIB graphical extension: gui-clipboard. C11, header-only. */
#ifndef MULIB_GUI_CLIPBOARD_H
#define MULIB_GUI_CLIPBOARD_H
#include "mulib-gui-mime.h"
#ifndef MULIB_GUI_CLIPBOARD_LIMIT
#define MULIB_GUI_CLIPBOARD_LIMIT (16u*1024u*1024u)
#endif
/* In-process owned bytes only: no Wayland/X11 clipboard transport. */
typedef struct{char mime[128];unsigned char *data;size_t n;uint64_t serial;}mulib_gui_clipboard_t;
static inline int mulib_gui_clipboard_set(mulib_gui_clipboard_t *c,const char *mime,const void *p,size_t n){
 if(!c||!mulib_gui_mime_valid(mime)||(!p&&n)||n>MULIB_GUI_CLIPBOARD_LIMIT||c->serial==UINT64_MAX)return 0;
 char type[128];memcpy(type,mime,strlen(mime)+1);
 unsigned char *q=malloc(n?n:1);if(!q)return 0;
 if(n)memcpy(q,p,n);
 /* Both MIME and data may alias the previous clipboard's allocation. */
 free(c->data);c->data=q;c->n=n;memcpy(c->mime,type,strlen(type)+1);c->serial++;return 1;
}
static inline const void *mulib_gui_clipboard_get(const mulib_gui_clipboard_t *c,const char *mime,size_t *n){
 if(n)*n=0;
 if(!c||!mulib_gui_mime_valid(mime)||strcmp(c->mime,mime))return NULL;
 if(n)*n=c->n;
 return c->data;
}
static inline void mulib_gui_clipboard_free(mulib_gui_clipboard_t *c){if(c){free(c->data);memset(c,0,sizeof *c);}}
#endif
