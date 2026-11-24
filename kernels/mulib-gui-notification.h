/* MuLIB graphical extension: gui-notification. C11, header-only. */
#ifndef MULIB_GUI_NOTIFICATION_H
#define MULIB_GUI_NOTIFICATION_H
#include "mulib-gui-core.h"
typedef struct{uint64_t id;char title[128],body[512];double expires;}mgui_notification;
typedef struct{mgui_notification *v;size_t n,cap;uint64_t next;}mulib_gui_notification_t;
static inline uint64_t mulib_gui_notification_post(mulib_gui_notification_t *q,const char *title,const char *body,double expiry){if(!title||!body||!isfinite(expiry)||q->next==UINT64_MAX||!mgx_grow((void**)&q->v,&q->cap,q->n+1,sizeof *q->v))return 0;mgui_notification *n=q->v+q->n++;n->id=++q->next;n->expires=expiry;snprintf(n->title,sizeof n->title,"%s",title);snprintf(n->body,sizeof n->body,"%s",body);return n->id;}
static inline void mulib_gui_notification_expire(mulib_gui_notification_t *q,double now){size_t n=0;for(size_t i=0;i<q->n;i++)if(q->v[i].expires>now)q->v[n++]=q->v[i];q->n=n;}
static inline void mulib_gui_notification_free(mulib_gui_notification_t *q){free(q->v);memset(q,0,sizeof *q);}
#endif
