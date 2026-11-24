/* MuLIB graphical extension: glx-swapchain. C11, header-only. */
#ifndef MULIB_GLX_SWAPCHAIN_H
#define MULIB_GLX_SWAPCHAIN_H
#include "mulib-glx-surface.h"
typedef struct {
 mgx_surface image[3];
 unsigned count,front,back;
 int acquired;
 unsigned busy[3];
} mulib_glx_swapchain_t;
/* Initialize an empty object. Owned swapchains must not be copied. */
static inline int
mulib_glx_swapchain_init(mulib_glx_swapchain_t *s,int w,int h,unsigned n)
{
 mulib_glx_swapchain_t next={0};
 if(!s||n<2||n>3)return 0;
 for(unsigned i=0;i<n;i++){
  if(!mgx_surface_init(&next.image[i],w,h)){
   for(unsigned j=0;j<i;j++)mgx_surface_free(&next.image[j]);
   return 0;
  }
 }
 next.count=n;
 next.back=1;
 *s=next;
 return 1;
}
static inline mgx_surface *
mulib_glx_swapchain_acquire(mulib_glx_swapchain_t *s)
{
 if(!s||s->count<2||s->count>3||s->acquired||s->back>=s->count)return NULL;
 for(unsigned i=0;i<s->count;i++){
  unsigned index=(s->back+i)%s->count;
  if(!s->busy[index]){
   s->back=index;
   s->acquired=1;
   return &s->image[index];
  }
 }
 return NULL;
}
static inline int
mulib_glx_swapchain_cancel(mulib_glx_swapchain_t *s)
{
 if(!s||!s->acquired)return 0;
 s->acquired=0;
 return 1;
}
static inline int
mulib_glx_swapchain_submit_mode(mulib_glx_swapchain_t *s,int asynchronous)
{
 if(!s||s->count<2||s->count>3||!s->acquired||
    s->back>=s->count||s->busy[s->back])return 0;
 s->front=s->back;
 s->busy[s->front]=asynchronous!=0;
 s->back=(s->back+1)%s->count;
 s->acquired=0;
 return 1;
}
static inline int mulib_glx_swapchain_submit(mulib_glx_swapchain_t *s)
{
 return mulib_glx_swapchain_submit_mode(s,0);
}
static inline int
mulib_glx_swapchain_complete(mulib_glx_swapchain_t *s,unsigned index)
{
 if(!s||index>=s->count||index>=3||!s->busy[index])return 0;
 s->busy[index]=0;
 return 1;
}
static inline int
mulib_glx_swapchain_try_free(mulib_glx_swapchain_t *s)
{
 if(!s||s->count>3)return 0;
 for(unsigned i=0;i<s->count;i++)
  if(s->busy[i]){errno=EBUSY;return 0;}
 for(unsigned i=0;i<s->count;i++)mgx_surface_free(&s->image[i]);
 memset(s,0,sizeof *s);
 return 1;
}
static inline void mulib_glx_swapchain_free(mulib_glx_swapchain_t *s)
{
 (void)mulib_glx_swapchain_try_free(s);
}
#endif
