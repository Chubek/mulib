/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_MIME_H
#define MULIB_NB_MIME_H
#include "mulib-nb-output.h"
typedef int (*mulib_nb_mime_fn)(void *,const mulib_nb_output *);
typedef struct {const char *mime;int active;mulib_nb_mime_fn render;void *user;} mulib_nb_mime_handler;
static inline int mulib_nb_mime_render(const mulib_nb_mime_handler *v,size_t n,const mulib_nb_output *o,int trusted){if(!v||!o||!o->mime)return 0;for(size_t i=0;i<n;i++)if(v[i].mime&&!strcmp(v[i].mime,o->mime)&&v[i].render&&(!v[i].active||trusted))return v[i].render(v[i].user,o);return 0;}
#endif
