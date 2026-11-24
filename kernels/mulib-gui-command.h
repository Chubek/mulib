/* MuLIB graphical extension: gui-command. C11, header-only. */
#ifndef MULIB_GUI_COMMAND_H
#define MULIB_GUI_COMMAND_H
#include "mulib-gui-core.h"
typedef int (*mgui_command_fn)(void *);typedef struct{uint64_t id;mgui_command_fn run,enabled;void *user;}mgui_command;
typedef struct{mgui_command *v;size_t n,cap;}mulib_gui_command_t;
static inline int mulib_gui_command_register(mulib_gui_command_t *c,mgui_command a){if(!a.id||!a.run)return 0;for(size_t i=0;i<c->n;i++)if(c->v[i].id==a.id){c->v[i]=a;return 1;}if(!mgx_grow((void**)&c->v,&c->cap,c->n+1,sizeof a))return 0;c->v[c->n++]=a;return 1;}
static inline int mulib_gui_command_run(mulib_gui_command_t *c,uint64_t id){for(size_t i=0;i<c->n;i++)if(c->v[i].id==id){mgui_command a=c->v[i];return (!a.enabled||a.enabled(a.user))&&a.run(a.user);}return 0;}
static inline void mulib_gui_command_free(mulib_gui_command_t *c){free(c->v);memset(c,0,sizeof *c);}
#endif
