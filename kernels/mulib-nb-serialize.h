/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_SERIALIZE_H
#define MULIB_NB_SERIALIZE_H
#include "mulib-nb-tree.h"
#include <stdio.h>
/* MNB1: little endian u64 fields; heights in thousandths of a pixel. No trust/native handles. */
static inline int mulib_nb_put64(FILE *f,uint64_t x){unsigned char b[8];for(unsigned i=0;i<8;i++)b[i]=(unsigned char)(x>>(i*8));return fwrite(b,1,8,f)==8;}
static inline int mulib_nb_get64(FILE *f,uint64_t *x){unsigned char b[8];if(fread(b,1,8,f)!=8)return 0;*x=0;for(unsigned i=0;i<8;i++)*x|=(uint64_t)b[i]<<(i*8);return 1;}
static inline int mulib_nb_putstr(FILE *f,const char *s){size_t n=strlen(s);return mulib_nb_put64(f,n)&&fwrite(s,1,n,f)==n;}
static inline char *mulib_nb_getstr(FILE *f,size_t *budget){uint64_t n;if(!mulib_nb_get64(f,&n)||n>=SIZE_MAX||n>=*budget)return NULL;char *s=(char*)malloc((size_t)n+1);if(!s)return NULL;if(fread(s,1,(size_t)n,f)!=n||memchr(s,0,(size_t)n)){free(s);return NULL;}s[n]=0;*budget-=(size_t)n+1;return s;}
static inline int mulib_nb_write(FILE *f,const mulib_nb *n){if(!f||!n||!n->name||fwrite("MNB1",1,4,f)!=4||!mulib_nb_put64(f,n->id)||!mulib_nb_put64(f,n->next)||!mulib_nb_put64(f,n->n)||!mulib_nb_putstr(f,n->name))return 0;for(size_t i=0;i<n->n;i++){const mulib_nb_cell *c=n->v+i;if(!isfinite(c->height)||c->height<0.001||c->height>1e9||!mulib_nb_put64(f,c->id)||!mulib_nb_put64(f,c->parent)||!mulib_nb_put64(f,c->kind)||!mulib_nb_put64(f,(uint64_t)(c->height*1000+0.5))||!mulib_nb_put64(f,c->flags&MULIB_NB_COLLAPSED)||!mulib_nb_putstr(f,c->source)||!mulib_nb_putstr(f,c->language))return 0;}return !ferror(f);}
/* out must be empty. Limits bound cell count and allocated string bytes. */
static inline int mulib_nb_read(FILE *f,mulib_nb *out,size_t cell_limit,size_t budget){uint64_t id,next,count;char magic[4];if(!f||!out||fread(magic,1,4,f)!=4||memcmp(magic,"MNB1",4)||!mulib_nb_get64(f,&id)||!id||!mulib_nb_get64(f,&next)||!mulib_nb_get64(f,&count)||count>cell_limit||count>next)return 0;mulib_nb t={.id=id};t.name=mulib_nb_getstr(f,&budget);if(!t.name)return 0;for(uint64_t i=0;i<count;i++){uint64_t cid,p,k,h,flags;if(!mulib_nb_get64(f,&cid)||!cid||cid>next||mulib_nb_find(&t,cid)||!mulib_nb_get64(f,&p)||!mulib_nb_get64(f,&k)||k<1||k>MULIB_NB_RAW||!mulib_nb_get64(f,&h)||!h||h>UINT64_C(1000000000000)||!mulib_nb_get64(f,&flags)||flags>1)goto fail;char *s=mulib_nb_getstr(f,&budget),*l=s?mulib_nb_getstr(f,&budget):NULL;if(!s||!l||!mulib_nb_insert(&t,t.n,(mulib_nb_kind)k,s,l,h/1000.0)){free(s);free(l);goto fail;}free(s);free(l);t.v[t.n-1].id=cid;t.v[t.n-1].parent=p;t.v[t.n-1].flags=(unsigned)flags;}for(size_t i=0;i<t.n;i++)if(!mulib_nb_reparent(&t,t.v[i].id,t.v[i].parent))goto fail;t.next=next;*out=t;return 1;fail:mulib_nb_free(&t);return 0;}
#endif
