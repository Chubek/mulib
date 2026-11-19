/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_DMABUF_H
#define MULIB_DMABUF_H
#include "mulib-wm-core.h"
#include <unistd.h>
#include <fcntl.h>
typedef struct {int fd;uint32_t offset,stride;uint64_t modifier;} mulib_dmabuf_plane;
typedef struct {int32_t w,h;uint32_t format;unsigned n;mulib_dmabuf_plane planes[4];} mulib_dmabuf;
static inline int mulib_dmabuf_valid(const mulib_dmabuf *b){if(!b||b->w<=0||b->h<=0||!b->format||!b->n||b->n>4)return 0;for(unsigned i=0;i<b->n;i++)if(b->planes[i].fd<0||!b->planes[i].stride||b->planes[i].modifier!=b->planes[0].modifier)return 0;return 1;}
static inline void mulib_dmabuf_free(mulib_dmabuf *b){if(b){for(unsigned i=0;i<b->n&&i<4;i++)if(b->planes[i].fd>=0){int fd=b->planes[i].fd;for(unsigned j=i;j<b->n&&j<4;j++)if(b->planes[j].fd==fd)b->planes[j].fd=-1;close(fd);}memset(b,0,sizeof *b);}}
static inline int mulib_dmabuf_clone(const mulib_dmabuf *b,mulib_dmabuf *out){if(!out||b==out||!mulib_dmabuf_valid(b))return 0;mulib_dmabuf t=*b;t.n=0;for(unsigned i=0;i<b->n;i++){int fd=fcntl(b->planes[i].fd,F_DUPFD_CLOEXEC,0);if(fd<0){mulib_dmabuf_free(&t);return 0;}t.planes[i].fd=fd;t.n++;}*out=t;return 1;}
#endif
