/* MuLIB graphical extension: retgtk-object. C11, header-only. */
#ifndef MULIB_RETGTK_OBJECT_H
#define MULIB_RETGTK_OBJECT_H
#include "mulib-gui-core.h"
typedef struct mulib_retgtk_weak mulib_retgtk_weak_t;
typedef struct mulib_retgtk_object {
    size_t refs;
    uint64_t id;
    void (*finalize)(struct mulib_retgtk_object *);
    mulib_retgtk_weak_t *weak;
} mulib_retgtk_object_t;
/* Weak links and owning objects must not move in memory while linked. UI-thread only. */
struct mulib_retgtk_weak {
    mulib_retgtk_object_t *object;
    mulib_retgtk_weak_t *next, **prev;
};
static inline void mulib_retgtk_object_init(mulib_retgtk_object_t *o,uint64_t id,void (*fn)(mulib_retgtk_object_t *)){
    if(o){memset(o,0,sizeof *o);o->refs=1;o->id=id;o->finalize=fn;}
}
static inline void *mulib_retgtk_object_ref(mulib_retgtk_object_t *o){if(!o||!o->refs||o->refs==SIZE_MAX)return NULL;o->refs++;return o;}
static inline void mulib_retgtk_weak_clear(mulib_retgtk_weak_t *w){
    if(!w)return;
    if(w->prev){*w->prev=w->next;if(w->next)w->next->prev=w->prev;}
    memset(w,0,sizeof *w);
}
static inline int mulib_retgtk_weak_set(mulib_retgtk_weak_t *w,mulib_retgtk_object_t *o){
    if(!w||(o&&!o->refs))return 0;
    mulib_retgtk_weak_clear(w);
    if(o){w->object=o;w->next=o->weak;w->prev=&o->weak;if(w->next)w->next->prev=&w->next;o->weak=w;}
    return 1;
}
static inline void *mulib_retgtk_weak_lock(const mulib_retgtk_weak_t *w){
    return w?mulib_retgtk_object_ref(w->object):NULL;
}
static inline void mulib_retgtk_object_unref(mulib_retgtk_object_t *o){
    if(!o||!o->refs||--o->refs)return;
    while(o->weak)mulib_retgtk_weak_clear(o->weak);
    if(o->finalize)o->finalize(o);
}
#endif
