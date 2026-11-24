/* MuLIB graphical extension: gui-resource. C11, header-only. */
#ifndef MULIB_GUI_RESOURCE_H
#define MULIB_GUI_RESOURCE_H
#include "mulib-gui-core.h"
#ifndef MULIB_GUI_RESOURCE_LIMIT
#define MULIB_GUI_RESOURCE_LIMIT 4096u
#endif
typedef void (*mgui_release_fn)(void *);typedef struct{uint64_t id;void *data;mgui_release_fn release;size_t refs;}mgui_resource;
typedef struct{mgui_resource *v;size_t n,cap;int closing;}mulib_gui_resource_t;
/* add transfers one owned reference only on success. The registry must remain
 * alive during callbacks; callbacks may reenter it but must not free its memory.
 * free invalidates ALL outstanding acquired pointers, regardless of refcount. */
static inline int mulib_gui_resource_add(mulib_gui_resource_t *r,uint64_t id,void *p,mgui_release_fn fn){
 if(!r||r->closing||!id||!p||r->n>=MULIB_GUI_RESOURCE_LIMIT||r->n==SIZE_MAX)return 0;
 for(size_t i=0;i<r->n;i++)if(r->v[i].id==id)return 0;
 if(!mgx_grow((void**)&r->v,&r->cap,r->n+1,sizeof *r->v))return 0;
 r->v[r->n++]=(mgui_resource){id,p,fn,1};return 1;
}
static inline void *mulib_gui_resource_acquire(mulib_gui_resource_t *r,uint64_t id){
 if(!r||r->closing||!id)return NULL;
 for(size_t i=0;i<r->n;i++)if(r->v[i].id==id&&r->v[i].refs&&r->v[i].refs<SIZE_MAX){r->v[i].refs++;return r->v[i].data;}
 return NULL;
}
static inline int mulib_gui_resource_release(mulib_gui_resource_t *r,uint64_t id){
 if(!r||r->closing||!id)return 0;
 for(size_t i=0;i<r->n;i++)if(r->v[i].id==id){
  if(!r->v[i].refs)return 0;
  if(!--r->v[i].refs){
   mgui_resource t=r->v[i];r->v[i]=r->v[--r->n];
   /* Unlink before calling user code: it may remove, add, or free resources. */
   if(t.release)t.release(t.data);
  }
  return 1;
 }
 return 0;
}
static inline void mulib_gui_resource_free(mulib_gui_resource_t *r){
 if(!r||r->closing)return;
 mgui_resource *v=r->v;size_t n=r->n;
 r->v=NULL;r->n=0;r->cap=0;r->closing=1;
 while(n){mgui_resource t=v[--n];if(t.release)t.release(t.data);}
 free(v);r->closing=0;
}
#endif
