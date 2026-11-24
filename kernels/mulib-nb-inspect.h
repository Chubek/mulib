/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_INSPECT_H
#define MULIB_NB_INSPECT_H
#include "mulib-wm-core.h"
typedef struct {const char *name,*type,*documentation,*representation;} mulib_nb_symbol;
static inline const mulib_nb_symbol *mulib_nb_inspect(const mulib_nb_symbol *v,size_t n,const char *name){if(v&&name)for(size_t i=0;i<n;i++)if(v[i].name&&!strcmp(name,v[i].name))return v+i;return NULL;}
static inline size_t mulib_nb_inspect_prefix(const mulib_nb_symbol *v,size_t n,const char *prefix,const mulib_nb_symbol **out,size_t cap){if(!v||!prefix||(!out&&cap))return 0;size_t len=strlen(prefix),k=0;for(size_t i=0;i<n&&k<cap;i++)if(v[i].name&&!strncmp(v[i].name,prefix,len))out[k++]=v+i;return k;}
#endif
