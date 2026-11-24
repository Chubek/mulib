/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_IPYNB_H
#define MULIB_NB_IPYNB_H
#include "mulib-nb-core.h"
#include <stdio.h>
#include <inttypes.h>
/* nbformat 4.5 export; non-code native cells become raw text, never executable. */
static inline int mulib_nb_json_string(FILE *f,const char *s){if(!f||!s||fputc('"',f)==EOF)return 0;for(const unsigned char *p=(const unsigned char*)s;*p;p++){unsigned c=*p;if(c=='"'||c=='\\'){if(fputc('\\',f)==EOF||fputc((int)c,f)==EOF)return 0;}else if(c<32){if(fprintf(f,"\\u%04x",c)<0)return 0;}else if(fputc((int)c,f)==EOF)return 0;}return fputc('"',f)!=EOF;}
static inline int mulib_nb_ipynb_write(FILE *f,const mulib_nb *n){if(!f||!n||fputs("{\"nbformat\":4,\"nbformat_minor\":5,\"metadata\":{},\"cells\":[",f)==EOF)return 0;for(size_t i=0;i<n->n;i++){const mulib_nb_cell *c=n->v+i;const char *kind=c->kind==MULIB_NB_CODE?"code":c->kind==MULIB_NB_MARKDOWN?"markdown":"raw";if(fprintf(f,"%s{\"id\":\"c%" PRIx64 "\",\"cell_type\":\"%s\",\"metadata\":{\"mulib_kind\":%u,\"language\":",i?",":"",c->id,kind,(unsigned)c->kind)<0||!mulib_nb_json_string(f,c->language)||fputs("},\"source\":",f)==EOF||!mulib_nb_json_string(f,c->source))return 0;if(c->kind==MULIB_NB_CODE&&fputs(",\"execution_count\":null,\"outputs\":[]",f)==EOF)return 0;if(fputc('}',f)==EOF)return 0;}return fputs("]}\n",f)!=EOF&&!ferror(f);}
#endif
