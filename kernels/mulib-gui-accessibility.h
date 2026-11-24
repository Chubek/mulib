/* MuLIB graphical extension: gui-accessibility. C11, header-only. */
#ifndef MULIB_GUI_ACCESSIBILITY_H
#define MULIB_GUI_ACCESSIBILITY_H
#include "mulib-gui-core.h"
typedef enum{MG_ACC_WINDOW,MG_ACC_BUTTON,MG_ACC_LABEL,MG_ACC_ENTRY,MG_ACC_CHECK,MG_ACC_SLIDER,MG_ACC_LIST}mgui_access_role;
typedef struct{uint64_t id,parent;mgui_access_role role;mgx_rect bounds;char name[128],value[128];unsigned state;int (*action)(void *,uint64_t,unsigned);void *user;}mgui_access_node;
typedef struct{mgui_access_node *v;size_t n,cap;}mulib_gui_accessibility_t;
static inline int mulib_gui_accessibility_add(mulib_gui_accessibility_t *a,mgui_access_node n){if(!n.id)return 0;for(size_t i=0;i<a->n;i++)if(a->v[i].id==n.id){a->v[i]=n;return 1;}if(!mgx_grow((void**)&a->v,&a->cap,a->n+1,sizeof n))return 0;a->v[a->n++]=n;return 1;}
static inline const mgui_access_node *mulib_gui_accessibility_find(const mulib_gui_accessibility_t *a,uint64_t id){for(size_t i=0;i<a->n;i++)if(a->v[i].id==id)return a->v+i;return NULL;}
static inline int mulib_gui_accessibility_action(const mulib_gui_accessibility_t *a,uint64_t id,unsigned action){const mgui_access_node *n=mulib_gui_accessibility_find(a,id);return n&&n->action&&n->action(n->user,id,action);}
static inline void mulib_gui_accessibility_free(mulib_gui_accessibility_t *a){free(a->v);memset(a,0,sizeof *a);}
#endif
