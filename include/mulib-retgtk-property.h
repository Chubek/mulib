/* MuLIB graphical extension: retgtk-property. C11, header-only. */
#ifndef MULIB_RETGTK_PROPERTY_H
#define MULIB_RETGTK_PROPERTY_H
#include "mulib-retgtk-signal.h"
typedef enum{MRET_INT,MRET_DOUBLE,MRET_BOOL,MRET_STRING,MRET_POINTER}mret_value_kind;
typedef struct{mret_value_kind kind;union{int64_t i;double d;char *s;void *p;}as;}mret_value;
typedef struct{mret_value value;mulib_retgtk_signal_t signals;int disposed;}mulib_retgtk_property_t;
static inline void mulib_retgtk_value_clear(mret_value *v){
    if(!v)return;
    if(v->kind==MRET_STRING)free(v->as.s);
    memset(v,0,sizeof *v);
}
/* Destination must be empty. Pointers are borrowed, strings are copied. */
static inline int mulib_retgtk_value_copy(mret_value *out,mret_value v){
    if(!out||v.kind<MRET_INT||v.kind>MRET_POINTER)return 0;
    if(v.kind==MRET_BOOL)v.as.i=!!v.as.i;
    if(v.kind==MRET_STRING){
        const char *s=v.as.s?v.as.s:"";size_t n=strlen(s);
        if(n==SIZE_MAX)return 0;
        char *copy=malloc(n+1);if(!copy)return 0;
        memcpy(copy,s,n+1);v.as.s=copy;
    }
    *out=v;return 1;
}
static inline int mulib_retgtk_value_equal(mret_value a,mret_value b){
    if(a.kind!=b.kind)return 0;
    switch(a.kind){
    case MRET_INT:return a.as.i==b.as.i;
    case MRET_BOOL:return !!a.as.i==!!b.as.i;
    case MRET_DOUBLE:return a.as.d==b.as.d||(isnan(a.as.d)&&isnan(b.as.d));
    case MRET_STRING:return !strcmp(a.as.s?a.as.s:"",b.as.s?b.as.s:"");
    case MRET_POINTER:return a.as.p==b.as.p;
    default:return 0;
    }
}
static inline int mulib_retgtk_property_set(mulib_retgtk_property_t *p,mret_value v){
    if(!p||p->disposed||v.kind<MRET_INT||v.kind>MRET_POINTER)return 0;
    if(mulib_retgtk_value_equal(p->value,v))return 1;
    mret_value owned={0},notice={0};
    if(!mulib_retgtk_value_copy(&owned,v))return 0;
    if(!mulib_retgtk_value_copy(&notice,owned)){mulib_retgtk_value_clear(&owned);return 0;}
    mulib_retgtk_object_t *owner=p->signals.owner;
    if(owner&&!mulib_retgtk_object_ref(owner)){
        mulib_retgtk_value_clear(&owned);mulib_retgtk_value_clear(&notice);return 0;
    }
    mulib_retgtk_value_clear(&p->value);p->value=owned;
    /* An independent notification snapshot survives recursive set/free. */
    mulib_retgtk_signal_emit(&p->signals,p,1,&notice);
    mulib_retgtk_value_clear(&notice);
    mulib_retgtk_object_unref(owner);
    return 1;
}
static inline void mulib_retgtk_property_free(mulib_retgtk_property_t *p){
    if(!p||p->disposed)return;
    p->disposed=1;mulib_retgtk_value_clear(&p->value);mulib_retgtk_signal_free(&p->signals);
}
#endif
