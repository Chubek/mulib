/* MuLIB graphical extension: glx-platform. C11, header-only. */
#ifndef MULIB_GLX_PLATFORM_H
#define MULIB_GLX_PLATFORM_H
#include "mulib-glx-event.h"
#include "mulib-glx-display.h"
typedef struct mulib_glx_platform mulib_glx_platform_t;
typedef struct{int (*present)(void *,const mgx_surface *,const mgx_rect *,size_t);int (*poll)(void *,mulib_glx_event_queue_t *,int);void (*close)(void *);}mulib_glx_platform_ops_t;
struct mulib_glx_platform{void *user;mulib_glx_platform_ops_t ops;uint32_t capabilities;};
enum {
 MGX_CAP_WINDOWS=1, MGX_CAP_DIRECT=2, MGX_CAP_CLIPBOARD=4, MGX_CAP_INPUT=8,
 MGX_CAP_HEADLESS=16, MGX_CAP_ASYNC_PRESENT=32
};
/* A present callback borrows its arguments for the call. With ASYNC_PRESENT,
 * the surface pixels remain pinned until the caller acknowledges completion.
 * A missing callback is unsupported, except for explicitly headless platforms. */
static inline int
mulib_glx_platform_present(mulib_glx_platform_t *p,const mgx_surface *s,
                           const mgx_rect *r,size_t n)
{
 if(!p||!s||!s->pixels||s->w<=0||s->h<=0||s->stride<(size_t)s->w||
    s->stride>SIZE_MAX/(size_t)s->h/sizeof(uint32_t)||(n&&!r))
  return 0;
 if(p->ops.present)return p->ops.present(p->user,s,r,n);
 return (p->capabilities&MGX_CAP_HEADLESS)!=0 &&
        (p->capabilities&MGX_CAP_ASYNC_PRESENT)==0;
}
static inline void
mulib_glx_platform_headless(mulib_glx_platform_t *p)
{
 if(p){memset(p,0,sizeof *p);p->capabilities=MGX_CAP_HEADLESS;}
}
static inline void mulib_glx_platform_close(mulib_glx_platform_t *p){if(p&&p->ops.close)p->ops.close(p->user);if(p)memset(p,0,sizeof *p);}
#endif
