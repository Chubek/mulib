/* MuLIB graphical extension: glx-window. C11, header-only. */
#ifndef MULIB_GLX_WINDOW_H
#define MULIB_GLX_WINDOW_H
#include "mulib-glx-swapchain.h"
#include "mulib-glx-present.h"
typedef struct{mulib_glx_platform_t *platform;mulib_glx_swapchain_t swap;mulib_glx_present_t present;mgx_rect bounds;uint32_t id;int visible,closed;char title[256];}mulib_glx_window_t;
/* The platform is borrowed and must outlive the window and all pending frames. */
static inline int
mulib_glx_window_init(mulib_glx_window_t *w,mulib_glx_platform_t *p,
                      uint32_t id,const char *title,int width,int height)
{
 mulib_glx_window_t next={0};
 if(!w||!p||!mulib_glx_swapchain_init(&next.swap,width,height,2))return 0;
 next.platform=p;
 next.id=id;
 next.bounds=(mgx_rect){0,0,width,height};
 next.visible=1;
 snprintf(next.title,sizeof next.title,"%s",title?title:"");
 *w=next;
 return 1;
}
static inline int
mulib_glx_window_resize(mulib_glx_window_t *w,int x,int y)
{
 mulib_glx_swapchain_t next={0};
 if(!w||w->swap.acquired||w->present.pending)return 0;
 for(unsigned i=0;i<w->swap.count&&i<3;i++)if(w->swap.busy[i])return 0;
 if(!mulib_glx_swapchain_init(&next,x,y,2))return 0;
 if(!mulib_glx_swapchain_try_free(&w->swap)){
  mulib_glx_swapchain_free(&next);
  return 0;
 }
 w->swap=next;
 w->bounds.w=x;
 w->bounds.h=y;
 return 1;
}
static inline int
mulib_glx_window_present(mulib_glx_window_t *w)
{
 if(!w||!w->swap.acquired||w->closed||w->swap.count<2||
    w->swap.count>3||w->swap.back>=w->swap.count||
    w->swap.busy[w->swap.back])return 0;
 if(!mulib_glx_present_submit(&w->present,w->platform,
                              &w->swap.image[w->swap.back]))return 0;
 return mulib_glx_swapchain_submit_mode(&w->swap,w->present.pending);
}
static inline int
mulib_glx_window_complete(mulib_glx_window_t *w,uint64_t serial)
{
 if(!w||!w->present.pending||serial!=w->present.serial||
    w->swap.front>=w->swap.count||w->swap.front>=3||
    !w->swap.busy[w->swap.front])return 0;
 if(!mulib_glx_present_complete(&w->present,serial))return 0;
 return mulib_glx_swapchain_complete(&w->swap,w->swap.front);
}
static inline int
mulib_glx_window_try_free(mulib_glx_window_t *w)
{
 if(!w)return 0;
 if(w->present.pending){errno=EBUSY;return 0;}
 if(!mulib_glx_swapchain_try_free(&w->swap))return 0;
 (void)mulib_glx_present_try_free(&w->present);
 memset(w,0,sizeof *w);
 return 1;
}
static inline void mulib_glx_window_free(mulib_glx_window_t *w)
{
 (void)mulib_glx_window_try_free(w);
}
#endif
