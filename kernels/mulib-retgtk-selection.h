/* MuLIB graphical extension: retgtk-selection. C11, header-only. */
#ifndef MULIB_RETGTK_SELECTION_H
#define MULIB_RETGTK_SELECTION_H
#include "mulib-retgtk-model.h"
typedef struct{unsigned char *bits;size_t n,anchor;}mulib_retgtk_selection_t;
static inline int mulib_retgtk_selection_init(mulib_retgtk_selection_t *s,size_t n){unsigned char *p=calloc(n?n:1,1);if(!p)return 0;*s=(mulib_retgtk_selection_t){p,n,SIZE_MAX};return 1;}
static inline int mulib_retgtk_selection_choose(mulib_retgtk_selection_t *s,size_t i,int toggle,int range){if(i>=s->n)return 0;if(!toggle)memset(s->bits,0,s->n);if(range&&s->anchor<s->n){size_t a=i<s->anchor?i:s->anchor,b=i>s->anchor?i:s->anchor;memset(s->bits+a,1,b-a+1);}else{s->bits[i]=toggle?!s->bits[i]:1;s->anchor=i;}return 1;}
static inline void mulib_retgtk_selection_free(mulib_retgtk_selection_t *s){free(s->bits);memset(s,0,sizeof *s);}
#endif
