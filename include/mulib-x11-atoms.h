/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_ATOMS_H
#define MULIB_X11_ATOMS_H
#include "mulib-x11-ext.h"
#include "mulib-wm-core.h"
typedef struct {char *name;uint32_t atom;} mulib_x11_atom_entry;
typedef struct {mulib_x11_atom_entry *v;size_t n,cap;} mulib_x11_atoms;
static inline uint32_t mulib_x11_atoms_get(mulib_x11_atoms *a,mulib_x11_wm_io *io,const char *name,int existing){if(!a||!name||!*name)return 0;for(size_t i=0;i<a->n;i++)if(!strcmp(a->v[i].name,name))return a->v[i].atom;size_t n=strlen(name);if(n>65535||a->n==SIZE_MAX)return 0;size_t z=4+(n+3)/4*4;unsigned char *p=(unsigned char*)calloc(z,1),*r;size_t rn;if(!p)return 0;mulib_x11_wm_u16(p,(uint16_t)n);memcpy(p+4,name,n);int ok=mulib_x11_wm_reply(io,16,!!existing,p,z,&r,&rn);free(p);if(!ok)return 0;uint32_t id=mulib_x11_wm_r32(r+8);free(r);if(!id)return 0;char *s=mulib_wm_strdup(name);if(!s||!mulib_wm_grow((void**)&a->v,&a->cap,a->n+1,sizeof *a->v)){free(s);return 0;}a->v[a->n++]=(mulib_x11_atom_entry){s,id};return id;}
static inline void mulib_x11_atoms_free(mulib_x11_atoms *a){if(a){for(size_t i=0;i<a->n;i++)free(a->v[i].name);free(a->v);memset(a,0,sizeof *a);}}
#endif
