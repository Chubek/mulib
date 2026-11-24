/* MuLIB graphical extension: retgtk-reactive. C11, header-only. */
#ifndef MULIB_RETGTK_REACTIVE_H
#define MULIB_RETGTK_REACTIVE_H
#include "mulib-gui-core.h"
typedef struct{double value;size_t a,b;int op,dirty;}mret_reactive_node;
typedef struct{mret_reactive_node *v;size_t n,cap;}mulib_retgtk_reactive_t;
/* Nodes refer only to earlier nodes: op=0 input, 1=sum, 2=product, 3=difference, 4=division. */
static inline size_t mulib_retgtk_reactive_add(mulib_retgtk_reactive_t *r,int op,size_t a,size_t b,double value){if(!r||!isfinite(value)||op<0||op>4||r->n==SIZE_MAX||(op&&(a>=r->n||b>=r->n))||!mgx_grow((void**)&r->v,&r->cap,r->n+1,sizeof *r->v))return SIZE_MAX;size_t i=r->n++;r->v[i]=(mret_reactive_node){value,a,b,op,1};return i;}
static inline int mulib_retgtk_reactive_set(mulib_retgtk_reactive_t *r,size_t i,double value){if(!r||i>=r->n||r->v[i].op||!isfinite(value))return 0;if(r->v[i].value!=value){r->v[i].value=value;r->v[i].dirty=1;}return 1;}
/* A division by zero/overflow is represented as NaN; repeated NaN is unchanged.
   Publicly corrupted forward references/ops are rejected without reading out of bounds. */
static inline size_t mulib_retgtk_reactive_update(mulib_retgtk_reactive_t *r){
    if(!r)return 0;
    for(size_t i=0;i<r->n;i++)if(r->v[i].op<0||r->v[i].op>4||
        (r->v[i].op&&(r->v[i].a>=i||r->v[i].b>=i)))return 0;
    size_t changed=0;
    for(size_t i=0;i<r->n;i++){
        mret_reactive_node *v=r->v+i;
        if(v->op){
            double a=r->v[v->a].value,b=r->v[v->b].value;
            double c=v->op==1?a+b:v->op==2?a*b:v->op==3?a-b:b!=0?a/b:NAN;
            if(!isfinite(c))c=NAN;
            v->dirty|=!(v->value==c||(isnan(v->value)&&isnan(c)));v->value=c;
        }
        changed+=v->dirty!=0;v->dirty=0;
    }
    return changed;
}
static inline void mulib_retgtk_reactive_free(mulib_retgtk_reactive_t *r){if(r){free(r->v);memset(r,0,sizeof *r);}}
#endif
