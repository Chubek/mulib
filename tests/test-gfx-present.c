#include "mulib-glx-window.h"
#include <assert.h>

typedef struct {
 int accepted,calls;
 const mgx_surface *borrowed;
} backend_state;

static int
present(void *user,const mgx_surface *surface,const mgx_rect *damage,size_t n)
{
 backend_state *state=user;
 assert(surface&&surface->pixels);
 assert(!n||damage);
 ++state->calls;
 if(!state->accepted)return 0;
 state->borrowed=surface;
 return 1;
}

static void
test_submission(void)
{
 mgx_surface image={0};
 mulib_glx_present_t frame={0};
 backend_state state={0};
 mulib_glx_platform_t platform={0};
 assert(mgx_surface_init(&image,8,6));
 assert(!mulib_glx_platform_present(&platform,&image,NULL,0));
 assert(!mulib_glx_platform_present(NULL,&image,NULL,0));
 platform.user=&state;
 platform.ops.present=present;
 assert(mulib_glx_present_damage(&frame,(mgx_rect){1,1,2,2}));
 assert(!mulib_glx_present_submit(&frame,&platform,&image));
 assert(frame.serial==0&&frame.presented==0&&frame.damage.n==1);
 state.accepted=1;
 assert(mulib_glx_present_submit(&frame,&platform,&image));
 assert(frame.serial==1&&frame.presented==1&&!frame.pending&&!frame.damage.n);
 platform.capabilities=MGX_CAP_ASYNC_PRESENT;
 assert(mulib_glx_present_submit(&frame,&platform,&image));
 assert(frame.serial==2&&frame.presented==1&&frame.pending);
 assert(mulib_glx_present_damage(&frame,(mgx_rect){0,0,1,1}));
 assert(!mulib_glx_present_submit(&frame,&platform,&image));
 assert(!mulib_glx_present_try_free(&frame));
 assert(frame.pending&&frame.damage.n==1);
 assert(!mulib_glx_present_complete(&frame,1));
 assert(mulib_glx_present_complete(&frame,2));
 assert(!mulib_glx_present_complete(&frame,2));
 assert(frame.presented==2);
 frame.serial=UINT64_MAX;
 assert(!mulib_glx_present_submit(&frame,&platform,&image));
 assert(frame.damage.n==1);
 assert(mulib_glx_present_try_free(&frame));
 mulib_glx_platform_headless(&platform);
 assert(mulib_glx_present_submit(&frame,&platform,&image));
 assert(frame.presented==1&&!frame.pending);
 assert(!mulib_glx_platform_present(&platform,&image,NULL,1));
 mulib_glx_present_free(&frame);
 mgx_surface_free(&image);
}

static void
test_swapchain(void)
{
 mulib_glx_swapchain_t swap={0};
 unsigned first,second;
 assert(!mulib_glx_swapchain_init(NULL,4,4,2));
 assert(!mulib_glx_swapchain_init(&swap,4,4,4));
 assert(mulib_glx_swapchain_init(&swap,4,4,2));
 assert(mulib_glx_swapchain_acquire(&swap));
 first=swap.back;
 assert(!mulib_glx_swapchain_acquire(&swap));
 assert(mulib_glx_swapchain_submit_mode(&swap,1));
 assert(mulib_glx_swapchain_acquire(&swap));
 second=swap.back;
 assert(second!=first);
 assert(mulib_glx_swapchain_submit_mode(&swap,1));
 assert(!mulib_glx_swapchain_acquire(&swap));
 assert(!mulib_glx_swapchain_try_free(&swap));
 assert(mulib_glx_swapchain_complete(&swap,first));
 assert(!mulib_glx_swapchain_complete(&swap,first));
 assert(mulib_glx_swapchain_acquire(&swap)==&swap.image[first]);
 assert(mulib_glx_swapchain_cancel(&swap));
 assert(!mulib_glx_swapchain_cancel(&swap));
 assert(mulib_glx_swapchain_complete(&swap,second));
 assert(mulib_glx_swapchain_try_free(&swap));
}

static void
test_window(void)
{
 backend_state state={1,0,NULL};
 mulib_glx_platform_t platform={0};
 mulib_glx_window_t window={0};
 mgx_surface *next;
 uint32_t *pinned;
 platform.user=&state;
 platform.ops.present=present;
 platform.capabilities=MGX_CAP_ASYNC_PRESENT;
 assert(!mulib_glx_window_init(NULL,&platform,1,NULL,4,4));
 assert(mulib_glx_window_init(&window,&platform,1,"test",4,4));
 next=mulib_glx_swapchain_acquire(&window.swap);
 assert(next);
 pinned=next->pixels;
 mgx_clear(next,UINT32_C(0xff123456));
 assert(mulib_glx_window_present(&window));
 assert(state.borrowed->pixels==pinned&&window.present.pending);
 assert(!mulib_glx_window_resize(&window,8,8));
 assert(!mulib_glx_window_try_free(&window));
 assert(window.swap.image[window.swap.front].pixels==pinned);
 next=mulib_glx_swapchain_acquire(&window.swap);
 assert(next&&next->pixels!=pinned);
 assert(!mulib_glx_window_present(&window));
 assert(!mulib_glx_window_complete(&window,2));
 assert(mulib_glx_window_complete(&window,1));
 assert(*pinned==UINT32_C(0xff123456));
 assert(mulib_glx_window_present(&window));
 assert(mulib_glx_window_complete(&window,2));
 assert(mulib_glx_window_resize(&window,8,8));
 assert(window.bounds.w==8&&window.bounds.h==8);
 assert(mulib_glx_window_try_free(&window));
}

int
main(void)
{
 test_submission();
 test_swapchain();
 test_window();
 return 0;
}
