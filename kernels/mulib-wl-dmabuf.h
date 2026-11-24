/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_DMABUF_H
#define MULIB_WL_DMABUF_H
#include "mulib-dmabuf.h"
/* linux-dmabuf params accumulator: accepts owned FD only on success. Import supplied by renderer. */
typedef struct {mulib_dmabuf info;unsigned mask;int used;} mulib_wl_dmabuf_params;
static inline int mulib_wl_dmabuf_plane_add(mulib_wl_dmabuf_params *p,unsigned plane,int fd,uint32_t offset,uint32_t stride,uint64_t modifier){if(!p||p->used||plane>=4||fd<0||!stride||(p->mask&(1u<<plane)))return 0;int flags=fcntl(fd,F_GETFD);if(flags<0||fcntl(fd,F_SETFD,flags|FD_CLOEXEC)<0)return 0;p->info.planes[plane]=(mulib_dmabuf_plane){fd,offset,stride,modifier};p->mask|=1u<<plane;return 1;}
static inline int mulib_wl_dmabuf_create(mulib_wl_dmabuf_params *p,int32_t w,int32_t h,uint32_t format,mulib_dmabuf *out){if(!p||p->used||!out||!p->mask||(p->mask&(p->mask+1)))return 0;mulib_dmabuf b=p->info;b.w=w;b.h=h;b.format=format;b.n=0;for(unsigned m=p->mask;m;m>>=1)b.n++;if(!mulib_dmabuf_valid(&b))return 0;*out=b;p->mask=0;p->used=1;return 1;}
static inline void mulib_wl_dmabuf_params_free(mulib_wl_dmabuf_params *p){if(p){for(unsigned i=0;i<4;i++)if(p->mask&(1u<<i)){int fd=p->info.planes[i].fd;for(unsigned j=i+1;j<4;j++)if((p->mask&(1u<<j))&&p->info.planes[j].fd==fd)p->mask&=~(1u<<j);close(fd);}memset(p,0,sizeof *p);}}
#endif
