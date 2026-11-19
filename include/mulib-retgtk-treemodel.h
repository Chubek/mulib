/* MuLIB graphical extension: retgtk-treemodel. C11, header-only. */
#ifndef MULIB_RETGTK_TREEMODEL_H
#define MULIB_RETGTK_TREEMODEL_H
#include "mulib-retgtk-model.h"
#include <limits.h>
typedef struct{size_t parent;char *label;int expanded;}mret_tree_row;
typedef struct{mret_tree_row *v;size_t n,cap;}mulib_retgtk_treemodel_t;
static inline int mulib_retgtk_treemodel_add(mulib_retgtk_treemodel_t *m,size_t parent,const char *s){if(!m||m->n==SIZE_MAX||(parent!=SIZE_MAX&&parent>=m->n)||!s||!mgx_grow((void**)&m->v,&m->cap,m->n+1,sizeof *m->v))return 0;char *p=malloc(strlen(s)+1);if(!p)return 0;strcpy(p,s);m->v[m->n++]=(mret_tree_row){parent,p,0};return 1;}
static inline int mulib_retgtk_treemodel_visible(const mulib_retgtk_treemodel_t *m,size_t i,unsigned *depth){if(!m||i>=m->n)return 0;unsigned d=0;size_t child=i;for(size_t p=m->v[i].parent;p!=SIZE_MAX;p=m->v[p].parent){if(p>=child||!m->v[p].expanded||d==UINT_MAX)return 0;child=p;d++;}if(depth)*depth=d;return 1;}
static inline void mulib_retgtk_treemodel_free(mulib_retgtk_treemodel_t *m){for(size_t i=0;i<m->n;i++)free(m->v[i].label);free(m->v);memset(m,0,sizeof *m);}
#endif
