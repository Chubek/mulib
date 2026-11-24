/* MuLIB graphical extension: retgtk-signal. C11, header-only. */
#ifndef MULIB_RETGTK_SIGNAL_H
#define MULIB_RETGTK_SIGNAL_H
#include "mulib-retgtk-object.h"
typedef void (*mret_signal_fn)(void *,void *,unsigned,const void *);
typedef struct {
    uint64_t id;unsigned signal;mret_signal_fn fn;void *user;
    mulib_retgtk_weak_t *receiver;
} mret_slot;
typedef struct mulib_retgtk_signal mulib_retgtk_signal_t;
typedef struct mulib_retgtk_signal_watch {
    mulib_retgtk_signal_t *signal;
    struct mulib_retgtk_signal_watch *next, **prev;
} mulib_retgtk_signal_watch_t;
struct mulib_retgtk_signal {
    mret_slot *v;size_t n,cap;uint64_t next;unsigned emitting;
    int closing;
    mulib_retgtk_object_t *owner;
    mulib_retgtk_signal_watch_t *watches;
};
static inline void mulib_retgtk_signal_watch_clear(mulib_retgtk_signal_watch_t *w){
    if(!w)return;
    if(w->prev){*w->prev=w->next;if(w->next)w->next->prev=w->prev;}
    memset(w,0,sizeof *w);
}
static inline int mulib_retgtk_signal_watch_set(mulib_retgtk_signal_watch_t *w,mulib_retgtk_signal_t *s){
    if(!w||(s&&s->closing))return 0;
    mulib_retgtk_signal_watch_clear(w);
    if(s){w->signal=s;w->next=s->watches;w->prev=&s->watches;if(w->next)w->next->prev=&w->next;s->watches=w;}
    return 1;
}
/* The owner contains this signal; do not bind an unrelated or shorter-lived object. */
static inline int mulib_retgtk_signal_bind_owner(mulib_retgtk_signal_t *s,mulib_retgtk_object_t *o){
    if(!s||s->emitting||s->closing||(o&&!o->refs))return 0;
    s->owner=o;return 1;
}
static inline void mret_signal_compact(mulib_retgtk_signal_t *s){
    size_t k=0;
    for(size_t i=0;i<s->n;i++){
        mret_slot *p=&s->v[i];
        if(p->receiver&&!p->receiver->object)p->fn=NULL;
        if(p->fn)s->v[k++]=*p;
        else if(p->receiver){mulib_retgtk_weak_clear(p->receiver);free(p->receiver);}
    }
    s->n=k;
}
static inline uint64_t mulib_retgtk_signal_connect(mulib_retgtk_signal_t *s,unsigned sig,mret_signal_fn fn,void *u){
    if(!s||!fn||s->closing||(s->owner&&!s->owner->refs)||s->next==UINT64_MAX)return 0;
    if(!s->emitting)mret_signal_compact(s);
    if(s->n==SIZE_MAX||!mgx_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v))return 0;
    uint64_t id=++s->next;
    s->v[s->n++]=(mret_slot){id,sig,fn,u,NULL};return id;
}
/* The receiver is weakly tracked and pinned only while its callback runs. */
static inline uint64_t mulib_retgtk_signal_connect_object(mulib_retgtk_signal_t *s,unsigned sig,mret_signal_fn fn,void *u,mulib_retgtk_object_t *receiver){
    if(!receiver||!receiver->refs)return 0;
    mulib_retgtk_weak_t *weak=calloc(1,sizeof *weak);
    if(!weak)return 0;
    uint64_t id=mulib_retgtk_signal_connect(s,sig,fn,u);
    if(!id){free(weak);return 0;}
    mulib_retgtk_weak_set(weak,receiver);s->v[s->n-1].receiver=weak;return id;
}
static inline int mulib_retgtk_signal_disconnect(mulib_retgtk_signal_t *s,uint64_t id){
    if(!s||!id)return 0;
    for(size_t i=0;i<s->n;i++)if(s->v[i].id==id&&s->v[i].fn){s->v[i].fn=NULL;if(!s->emitting)mret_signal_compact(s);return 1;}
    return 0;
}
static inline void mret_signal_release_storage(mulib_retgtk_signal_t *s){
    /* Reset subscriptions, not the containing object's lifetime association. */
    mulib_retgtk_object_t *owner=s->owner;
    free(s->v);memset(s,0,sizeof *s);s->owner=owner;
}
static inline void mulib_retgtk_signal_emit(mulib_retgtk_signal_t *s,void *sender,unsigned sig,const void *data){
    if(!s||s->closing||s->emitting>=32)return;
    mulib_retgtk_object_t *owner=s->owner;
    if(owner&&!mulib_retgtk_object_ref(owner))return;
    size_t n=s->n;
    s->emitting++;
    /* Re-read the array each iteration: callbacks can disconnect or reallocate it.
       Newly connected slots wait until the next (possibly nested) emission. */
    for(size_t i=0;i<n&&!s->closing;i++){
        mret_slot p=s->v[i];
        if(!p.fn||p.signal!=sig)continue;
        mulib_retgtk_object_t *receiver=p.receiver?mulib_retgtk_weak_lock(p.receiver):NULL;
        if(p.receiver&&!receiver){s->v[i].fn=NULL;continue;}
        p.fn(p.user,sender,sig,data);
        mulib_retgtk_object_unref(receiver);
    }
    if(!--s->emitting){
        mret_signal_compact(s);
        if(s->closing)mret_signal_release_storage(s);
    }
    /* Last operation: the owner may contain and free s. */
    mulib_retgtk_object_unref(owner);
}
static inline void mulib_retgtk_signal_free(mulib_retgtk_signal_t *s){
    if(!s)return;
    while(s->watches)mulib_retgtk_signal_watch_clear(s->watches);
    for(size_t i=0;i<s->n;i++)s->v[i].fn=NULL;
    if(s->emitting){s->closing=1;return;}
    mret_signal_compact(s);mret_signal_release_storage(s);
}
#endif
