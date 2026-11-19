/* MuLIB graphical extension: gui-dispatch. C11, header-only. */
#ifndef MULIB_GUI_DISPATCH_H
#define MULIB_GUI_DISPATCH_H
#include "mulib-gui-core.h"
typedef void (*mgui_task_fn)(void *);
typedef struct{mgui_task_fn fn;void *user;}mgui_task;
typedef struct{mgui_task *v;size_t n,cap;int running;}mulib_gui_dispatch_t;
static inline int mulib_gui_dispatch_post(mulib_gui_dispatch_t *d,mgui_task_fn fn,void *u){if(!fn||!mgx_grow((void**)&d->v,&d->cap,d->n+1,sizeof *d->v))return 0;d->v[d->n++]=(mgui_task){fn,u};return 1;}
static inline size_t mulib_gui_dispatch_run(mulib_gui_dispatch_t *d,size_t budget){if(d->running)return 0;d->running=1;size_t n=0;while(d->n&&n<budget){mgui_task t=d->v[0];memmove(d->v,d->v+1,(--d->n)*sizeof t);t.fn(t.user);n++;}d->running=0;return n;}
static inline void mulib_gui_dispatch_free(mulib_gui_dispatch_t *d){if(!d->running){free(d->v);memset(d,0,sizeof *d);}}
#endif
