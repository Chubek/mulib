/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_GBM_H
#define MULIB_GBM_H
#include "mulib-dmabuf.h"
#include <dlfcn.h>
/* Optional Mesa GBM adapter. Caller supplies render-node FD; lifetime exceeds device. */
typedef struct {void *lib,*dev;void *(*create)(int);void (*destroy)(void *);void *(*bo_create)(void *,uint32_t,uint32_t,uint32_t,uint32_t);void (*bo_destroy)(void *);int (*bo_fd)(void *);uint32_t (*bo_stride)(void *);} mulib_gbm;
static inline int mulib_gbm_symbol(void *lib,const char *name,void *dst,size_t size){void *p=dlsym(lib,name);if(!p||size!=sizeof p)return 0;memcpy(dst,&p,size);return 1;}
static inline int mulib_gbm_open(mulib_gbm *g,int fd){if(!g||fd<0)return 0;mulib_gbm t={0};t.lib=dlopen("libgbm.so.1",RTLD_NOW|RTLD_LOCAL);if(!t.lib)return 0;
#define MULIB_GBM_LOAD(member,symbol) mulib_gbm_symbol(t.lib,symbol,&t.member,sizeof t.member)
if(!MULIB_GBM_LOAD(create,"gbm_create_device")||!MULIB_GBM_LOAD(destroy,"gbm_device_destroy")||!MULIB_GBM_LOAD(bo_create,"gbm_bo_create")||!MULIB_GBM_LOAD(bo_destroy,"gbm_bo_destroy")||!MULIB_GBM_LOAD(bo_fd,"gbm_bo_get_fd")||!MULIB_GBM_LOAD(bo_stride,"gbm_bo_get_stride")||!(t.dev=t.create(fd))){dlclose(t.lib);return 0;}
#undef MULIB_GBM_LOAD
*g=t;return 1;}
static inline void *mulib_gbm_alloc(mulib_gbm *g,uint32_t w,uint32_t h,uint32_t format,uint32_t usage){return g&&g->dev&&w&&h&&format?g->bo_create(g->dev,w,h,format,usage):NULL;}
static inline int mulib_gbm_export(mulib_gbm *g,void *bo,int32_t w,int32_t h,uint32_t format,mulib_dmabuf *out){if(!g||!g->dev||!bo||!out||w<=0||h<=0||!format)return 0;uint32_t stride=g->bo_stride(bo);int fd=g->bo_fd(bo);if(fd<0)return 0;if(!stride||fcntl(fd,F_SETFD,FD_CLOEXEC)<0){close(fd);return 0;}*out=(mulib_dmabuf){.w=w,.h=h,.format=format,.n=1,.planes={{fd,0,stride,UINT64_C(0x00ffffffffffffff)}}};return 1;}
static inline void mulib_gbm_bo_free(mulib_gbm *g,void *bo){if(g&&g->dev&&bo)g->bo_destroy(bo);}
static inline void mulib_gbm_close(mulib_gbm *g){if(g){if(g->dev)g->destroy(g->dev);if(g->lib)dlclose(g->lib);memset(g,0,sizeof *g);}}
#endif
