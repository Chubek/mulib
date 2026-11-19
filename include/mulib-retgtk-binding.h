/* MuLIB graphical extension: retgtk-binding. C11, header-only. */
#ifndef MULIB_RETGTK_BINDING_H
#define MULIB_RETGTK_BINDING_H
#include "mulib-retgtk-property.h"
typedef struct{
    mulib_retgtk_property_t *a,*b;uint64_t ab,ba;int updating;
    mulib_retgtk_signal_watch_t a_watch,b_watch;
    uint64_t generation;
}mulib_retgtk_binding_t;
/* Binding storage is caller-owned, zero-initialized, immovable, and must outlive
   in-flight callbacks. Endpoint property storage can be freed after property_free. */
static inline void mret_binding_changed(void *u,void *sender,unsigned sig,const void *data){
    mulib_retgtk_binding_t *b=u;(void)sig;
    if(!b||!data||b->updating||!b->a_watch.signal||!b->b_watch.signal)return;
    if(sender!=b->a&&sender!=b->b)return;
    uint64_t generation=b->generation;
    mulib_retgtk_property_t *target=sender==b->a?b->b:b->a;
    b->updating=1;
    mulib_retgtk_property_set(target,*(const mret_value*)data);
    /* A nested callback may disconnect/reinitialize this same binding. */
    if(b->generation==generation)b->updating=0;
}
static inline void mulib_retgtk_binding_free(mulib_retgtk_binding_t *b){
    if(!b)return;
    if(b->a_watch.signal&&b->ab)mulib_retgtk_signal_disconnect(b->a_watch.signal,b->ab);
    if(b->b_watch.signal&&b->ba)mulib_retgtk_signal_disconnect(b->b_watch.signal,b->ba);
    mulib_retgtk_signal_watch_clear(&b->a_watch);mulib_retgtk_signal_watch_clear(&b->b_watch);
    b->a=b->b=NULL;b->ab=b->ba=0;b->updating=0;b->generation++;
}
static inline int mulib_retgtk_binding_init(mulib_retgtk_binding_t *b,mulib_retgtk_property_t *a,mulib_retgtk_property_t *c,int both){
    if(!b||!a||!c||a==c||a->disposed||c->disposed)return 0;
    mulib_retgtk_binding_free(b);
    b->a=a;b->b=c;
    if(!mulib_retgtk_signal_watch_set(&b->a_watch,&a->signals)||
       !mulib_retgtk_signal_watch_set(&b->b_watch,&c->signals)||
       !(b->ab=mulib_retgtk_signal_connect(&a->signals,1,mret_binding_changed,b))||
       (both&&!(b->ba=mulib_retgtk_signal_connect(&c->signals,1,mret_binding_changed,b)))){
        mulib_retgtk_binding_free(b);return 0;
    }
    uint64_t generation=b->generation;
    b->updating=1;
    int ok=mulib_retgtk_property_set(c,a->value);
    if(b->generation!=generation)return 0;
    b->updating=0;
    if(!ok||!b->a_watch.signal||!b->b_watch.signal){mulib_retgtk_binding_free(b);return 0;}
    return 1;
}
#endif
