/* MuLIB graphical extension: retgtk-type. C11, header-only. */
#ifndef MULIB_RETGTK_TYPE_H
#define MULIB_RETGTK_TYPE_H
#include "mulib-gui-core.h"
typedef struct mulib_retgtk_type{const char *name;const struct mulib_retgtk_type *parent;size_t size;void (*init)(void *);}mulib_retgtk_type_t;
/* Qualified names plus size and ancestry are the identity, never TU-local addresses.
   Descriptors/names are immutable and must outlive their instances/registry. */
static inline int mulib_retgtk_type_equal(const mulib_retgtk_type_t *a,const mulib_retgtk_type_t *b){
    for(unsigned i=0;i<128;i++){
        if(!a||!b)return a==b;
        if(!a->name||!b->name||!a->name[0]||strcmp(a->name,b->name)||a->size!=b->size)return 0;
        a=a->parent;b=b->parent;
    }
    return !a&&!b;
}
static inline int mulib_retgtk_type_is(const mulib_retgtk_type_t *t,const mulib_retgtk_type_t *base){
    if(!base)return 0;
    for(unsigned i=0;t&&i<128;i++,t=t->parent)if(mulib_retgtk_type_equal(t,base))return 1;
    return 0;
}
typedef struct{const mulib_retgtk_type_t **v;size_t n,cap;}mulib_retgtk_type_registry_t;
static inline const mulib_retgtk_type_t *mulib_retgtk_type_register(mulib_retgtk_type_registry_t *r,const mulib_retgtk_type_t *t){
    if(!r||!t||!t->size||!mulib_retgtk_type_equal(t,t))return NULL;
    for(size_t i=0;i<r->n;i++)if(!strcmp(r->v[i]->name,t->name))return mulib_retgtk_type_equal(r->v[i],t)?r->v[i]:NULL;
    if(r->n==SIZE_MAX||!mgx_grow((void**)&r->v,&r->cap,r->n+1,sizeof *r->v))return NULL;
    r->v[r->n++]=t;return t;
}
static inline void mulib_retgtk_type_registry_free(mulib_retgtk_type_registry_t *r){
    if(r){free(r->v);memset(r,0,sizeof *r);}
}
static inline void *mulib_retgtk_type_create(const mulib_retgtk_type_t *t){if(!t||!t->size)return NULL;const mulib_retgtk_type_t *v[128];size_t n=0;for(const mulib_retgtk_type_t *p=t;p;p=p->parent){if(n==128||p->size>t->size)return NULL;v[n++]=p;}void *p=calloc(1,t->size);if(p)while(n){const mulib_retgtk_type_t *q=v[--n];if(q->init)q->init(p);}return p;}
#endif
