/* MuLIB graphical extension: retgtk-template. C11, header-only. */
#ifndef MULIB_RETGTK_TEMPLATE_H
#define MULIB_RETGTK_TEMPLATE_H
#include "mulib-textbuffer.h"
typedef struct{const char *name,*value;}mret_template_value;
/* Expands ${name}; leaves no partially allocated output on error. */
static inline char *mulib_retgtk_template_expand(const char *s,const mret_template_value *v,size_t n){mulib_textbuffer_t b={0};while(*s){const char *q=strstr(s,"${");if(!q){if(!mulib_textbuffer_replace(&b,b.n,b.n,s,strlen(s)))goto fail;break;}if(!mulib_textbuffer_replace(&b,b.n,b.n,s,(size_t)(q-s)))goto fail;const char *e=strchr(q+2,'}');if(!e)goto fail;size_t i=0,k=(size_t)(e-q-2);while(i<n&&(strlen(v[i].name)!=k||memcmp(v[i].name,q+2,k)))i++;if(i==n||!mulib_textbuffer_replace(&b,b.n,b.n,v[i].value,strlen(v[i].value)))goto fail;s=e+1;}if(!b.data&&!mulib_textbuffer_set(&b,"",0))goto fail;return b.data;fail:mulib_textbuffer_free(&b);return NULL;}
#endif
