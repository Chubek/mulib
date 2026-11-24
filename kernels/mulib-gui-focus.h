/* MuLIB graphical extension: gui-focus. C11, header-only. */
#ifndef MULIB_GUI_FOCUS_H
#define MULIB_GUI_FOCUS_H
#include "mulib-gui-core.h"
typedef struct{uint64_t focused,scope;uint64_t *ids;size_t n,cap;}mulib_gui_focus_t;
static inline int mulib_gui_focus_add(mulib_gui_focus_t *f,uint64_t id){if(!id)return 0;for(size_t i=0;i<f->n;i++)if(f->ids[i]==id)return 1;if(!mgx_grow((void**)&f->ids,&f->cap,f->n+1,8))return 0;f->ids[f->n++]=id;return 1;}
static inline uint64_t mulib_gui_focus_step(mulib_gui_focus_t *f,int reverse){if(!f->n)return f->focused=0;size_t i=0;while(i<f->n&&f->ids[i]!=f->focused)i++;i=i==f->n?(reverse?f->n-1:0):reverse?(i+f->n-1)%f->n:(i+1)%f->n;return f->focused=f->ids[i];}
static inline void mulib_gui_focus_free(mulib_gui_focus_t *f){free(f->ids);memset(f,0,sizeof *f);}
#endif
