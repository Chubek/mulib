/* MuLIB graphical extension: retgtk-mune. C11, header-only. */
#ifndef MULIB_RETGTK_MUNE_H
#define MULIB_RETGTK_MUNE_H
#include "mulib-retgtk-widget.h"
/* Host-neutral handle table for Mune native glue; never exports raw pointers to Lua. */
typedef struct{uint64_t handle;mulib_retgtk_widget_t *widget;}mret_mune_handle;
typedef struct{mret_mune_handle *v;size_t n,cap;uint64_t next;int closing;}mulib_retgtk_mune_t;
static inline uint64_t mulib_retgtk_mune_add(mulib_retgtk_mune_t *m,mulib_retgtk_widget_t *w){if(!m||m->closing||!w||m->n==SIZE_MAX||m->next>=UINT64_C(9007199254740991)||!mgx_grow((void**)&m->v,&m->cap,m->n+1,sizeof *m->v)||!mulib_retgtk_object_ref(&w->object))return 0;uint64_t k=++m->next;m->v[m->n++]=(mret_mune_handle){k,w};return k;}
/* get() is borrowed. Native glue must use acquire() across script callbacks. */
static inline mulib_retgtk_widget_t *mulib_retgtk_mune_get(const mulib_retgtk_mune_t *m,uint64_t id){if(!m||m->closing||!id)return NULL;for(size_t i=0;i<m->n;i++)if(m->v[i].handle==id)return m->v[i].widget;return NULL;}
static inline mulib_retgtk_widget_t *mulib_retgtk_mune_acquire(const mulib_retgtk_mune_t *m,uint64_t id){
    mulib_retgtk_widget_t *w=mulib_retgtk_mune_get(m,id);
    return w?mulib_retgtk_object_ref(&w->object):NULL;
}
static inline int mulib_retgtk_mune_drop(mulib_retgtk_mune_t *m,uint64_t id){
    if(!m||!id)return 0;
    for(size_t i=0;i<m->n;i++)if(m->v[i].handle==id){
        mulib_retgtk_widget_t *w=m->v[i].widget;
        /* Remove first: destruction can reenter this table. */
        m->v[i]=m->v[--m->n];mulib_retgtk_widget_unref(w);return 1;
    }
    return 0;
}
static inline void mulib_retgtk_mune_free(mulib_retgtk_mune_t *m){
    if(!m||m->closing)return;
    m->closing=1;
    while(m->n)mulib_retgtk_mune_drop(m,m->v[m->n-1].handle);
    free(m->v);m->v=NULL;m->cap=0;m->closing=0;
    /* Keep the sequence: stale numeric handles must not alias later widgets. */
}
#endif
