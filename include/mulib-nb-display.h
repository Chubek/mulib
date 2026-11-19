/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_DISPLAY_H
#define MULIB_NB_DISPLAY_H
#include "mulib-nb-output.h"
/* Transactional replacement of a display handle (first matching MIME/cell). */
static inline int mulib_nb_display_update(mulib_nb_outputs *o,uint64_t cell,uint64_t display,const char *mime,const void *p,size_t n){if(!o||!display||!mime||(!p&&n))return 0;for(size_t i=0;i<o->n;i++){mulib_nb_output *v=o->v+i;if(v->cell==cell&&v->display==display&&!strcmp(v->mime,mime)){if(o->bytes<v->n||o->bytes-v->n>o->limit||n>o->limit-(o->bytes-v->n))return 0;unsigned char *b=(unsigned char*)malloc(n?n:1);if(!b)return 0;if(n)memcpy(b,p,n);free(v->data);v->data=b;o->bytes=o->bytes-v->n+n;v->n=n;return 1;}}return mulib_nb_output_add(o,cell,display,mime,p,n);}
#endif
