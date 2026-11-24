/* MuLIB graphical extension: glx-present. C11, header-only. */
#ifndef MULIB_GLX_PRESENT_H
#define MULIB_GLX_PRESENT_H
#include "mulib-glx-platform.h"
#include "mulib-glx-region.h"
typedef struct {
 mulib_glx_region_t damage;
 uint64_t serial,presented;
 int pending;
} mulib_glx_present_t;
static inline int
mulib_glx_present_damage(mulib_glx_present_t *p,mgx_rect r)
{
 return p&&mulib_glx_region_add(&p->damage,r);
}
/* One asynchronous submission may be outstanding. Failed submissions preserve
 * both the serial and accumulated damage, allowing an exact retry. */
static inline int
mulib_glx_present_submit(mulib_glx_present_t *p,mulib_glx_platform_t *backend,
                         const mgx_surface *s)
{
 if(!p||!backend||p->pending||p->serial==UINT64_MAX)return 0;
 if(!mulib_glx_platform_present(backend,s,p->damage.v,p->damage.n))return 0;
 ++p->serial;
 p->pending=(backend->capabilities&MGX_CAP_ASYNC_PRESENT)!=0;
 if(!p->pending)p->presented=p->serial;
 p->damage.n=0;
 return 1;
}
/* Call only after a real backend completion event, not after queueing a frame. */
static inline int
mulib_glx_present_complete(mulib_glx_present_t *p,uint64_t serial)
{
 if(!p||!p->pending||serial!=p->serial)return 0;
 p->presented=serial;
 p->pending=0;
  return 1;
}
/* Explicit backend-loss path, NOT a timeout/retry shortcut. The caller must
 * first disconnect/stop the backend and prove it no longer reads submitted
 * pixels. Unlike complete(), abort does not claim presentation/completion.
 * Serial remains consumed; damage accumulated since submission is retained.
 * Repaint the full new target before submitting to a replacement backend. */
static inline int
mulib_glx_present_abort(mulib_glx_present_t *p,uint64_t serial)
{
 if(!p||!p->pending||serial!=p->serial)return 0;
 p->pending=0;
 return 1;
}
static inline int
mulib_glx_present_try_free(mulib_glx_present_t *p)
{
 if(!p)return 0;
 if(p->pending){errno=EBUSY;return 0;}
 mulib_glx_region_free(&p->damage);
 memset(p,0,sizeof *p);
 return 1;
}
/* A busy presentation is left intact; complete it and retry teardown. */
static inline void mulib_glx_present_free(mulib_glx_present_t *p)
{
 (void)mulib_glx_present_try_free(p);
}
#endif
