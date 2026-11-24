/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_SESSION_H
#define MULIB_WM_SESSION_H
#include "mulib-wm-core.h"
#include <stdio.h>
#include <inttypes.h>
/* Portable numeric placement records; never stores runtime native handles. */
typedef struct {uint64_t workspace,cell;mulib_wm_rect rect;unsigned flags;} mulib_wm_saved_window;
static inline int mulib_wm_session_write(FILE *f,const mulib_wm_core *c){if(!f||!c||fprintf(f,"MWM1 %zu\n",c->n)<0)return 0;for(size_t i=0;i<c->n;i++){const mulib_wm_window *w=c->v+i;if(fprintf(f,"%" PRIu64 " %" PRIu64 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %u\n",w->workspace,w->cell,w->rect.x,w->rect.y,w->rect.w,w->rect.h,w->flags)<0)return 0;}return !ferror(f);}

#include <errno.h>
static inline int mulib_wm_session_uint(FILE *f,uint64_t *out){char b[64],*end;if(fscanf(f,"%63s",b)!=1||b[0]=='-')return 0;errno=0;uintmax_t x=strtoumax(b,&end,10);if(errno||end==b||*end||x>UINT64_MAX)return 0;*out=(uint64_t)x;return 1;}
static inline int mulib_wm_session_int(FILE *f,int32_t *out){char b[64],*end;if(fscanf(f,"%63s",b)!=1)return 0;errno=0;intmax_t x=strtoimax(b,&end,10);if(errno||end==b||*end||x<INT32_MIN||x>INT32_MAX)return 0;*out=(int32_t)x;return 1;}
static inline int mulib_wm_session_read(FILE *f,size_t limit,mulib_wm_saved_window **out,size_t *count){if(!f||!out||!count)return 0;char magic[5];uint64_t size;if(fscanf(f,"%4s",magic)!=1||strcmp(magic,"MWM1")||!mulib_wm_session_uint(f,&size)||size>limit||size>SIZE_MAX/sizeof(mulib_wm_saved_window))return 0;size_t n=(size_t)size;mulib_wm_saved_window *v=n?(mulib_wm_saved_window*)calloc(n,sizeof *v):NULL;if(n&&!v)return 0;for(size_t i=0;i<n;i++){mulib_wm_saved_window *w=v+i;uint64_t flags;if(!mulib_wm_session_uint(f,&w->workspace)||!mulib_wm_session_uint(f,&w->cell)||!mulib_wm_session_int(f,&w->rect.x)||!mulib_wm_session_int(f,&w->rect.y)||!mulib_wm_session_int(f,&w->rect.w)||!mulib_wm_session_int(f,&w->rect.h)||!mulib_wm_session_uint(f,&flags)||flags>UINT_MAX||!mulib_wm_rect_valid(w->rect)){free(v);return 0;}w->flags=(unsigned)flags;}*out=v;*count=n;return 1;}
#endif
