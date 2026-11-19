/* MuLIB graphical extension: retgtk-listmodel. C11, header-only. */
#ifndef MULIB_RETGTK_LISTMODEL_H
#define MULIB_RETGTK_LISTMODEL_H
#include "mulib-retgtk-model.h"
typedef struct{char **v;size_t n,cap;}mulib_retgtk_listmodel_t;
static inline int mulib_retgtk_listmodel_append(mulib_retgtk_listmodel_t *m,const char *s){if(!m||!s||m->n==SIZE_MAX||!mgx_grow((void**)&m->v,&m->cap,m->n+1,sizeof *m->v))return 0;size_t n=strlen(s);char *p=malloc(n+1);if(!p)return 0;memcpy(p,s,n+1);m->v[m->n++]=p;return 1;}
static inline size_t mret_list_rows(void *u){return ((mulib_retgtk_listmodel_t*)u)->n;}
static inline size_t mret_list_cols(void *u){(void)u;return 1;}
static inline const char *mret_list_cell(void *u,size_t i,size_t j){mulib_retgtk_listmodel_t *m=u;return !j&&i<m->n?m->v[i]:NULL;}
static inline int mret_list_set(void *u,size_t i,size_t j,const char *s){mulib_retgtk_listmodel_t *m=u;if(!m||j||i>=m->n||!s)return 0;size_t n=strlen(s);char *p=malloc(n+1);if(!p)return 0;memcpy(p,s,n+1);free(m->v[i]);m->v[i]=p;return 1;}
static inline int mulib_retgtk_listmodel_remove(mulib_retgtk_listmodel_t *m,size_t i){if(!m||i>=m->n)return 0;free(m->v[i]);memmove(m->v+i,m->v+i+1,(m->n-i-1)*sizeof *m->v);--m->n;return 1;}
static inline mulib_retgtk_model_t mulib_retgtk_listmodel_interface(mulib_retgtk_listmodel_t *m){return (mulib_retgtk_model_t){m,mret_list_rows,mret_list_cols,mret_list_cell,mret_list_set,0};}
static inline void mulib_retgtk_listmodel_free(mulib_retgtk_listmodel_t *m){for(size_t i=0;i<m->n;i++)free(m->v[i]);free(m->v);memset(m,0,sizeof *m);}
#endif
