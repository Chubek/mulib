/* MuLIB graphical extension: gui-shortcut. C11, header-only. */
#ifndef MULIB_GUI_SHORTCUT_H
#define MULIB_GUI_SHORTCUT_H
#include "mulib-gui-core.h"
typedef struct{unsigned key,mods;uint64_t command;}mgui_shortcut;
typedef struct{mgui_shortcut *v;size_t n,cap;}mulib_gui_shortcut_t;
static inline int mulib_gui_shortcut_bind(mulib_gui_shortcut_t *s,unsigned key,unsigned mods,uint64_t cmd){for(size_t i=0;i<s->n;i++)if(s->v[i].key==key&&s->v[i].mods==mods){s->v[i].command=cmd;return 1;}if(!mgx_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v))return 0;s->v[s->n++]=(mgui_shortcut){key,mods,cmd};return 1;}
static inline uint64_t mulib_gui_shortcut_match(const mulib_gui_shortcut_t *s,const mgx_event *e){if(e->type==MGX_EVENT_KEY&&e->down)for(size_t i=0;i<s->n;i++)if(s->v[i].key==e->code&&s->v[i].mods==e->mods)return s->v[i].command;return 0;}
static inline void mulib_gui_shortcut_free(mulib_gui_shortcut_t *s){free(s->v);memset(s,0,sizeof *s);}
#endif
