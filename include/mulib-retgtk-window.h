/* MuLIB graphical extension: retgtk-window. C11, header-only. */
#ifndef MULIB_RETGTK_WINDOW_H
#define MULIB_RETGTK_WINDOW_H
#include "mulib-retgtk-scene.h"
#include "mulib-retgtk-event.h"
#include "mulib-glx-window.h"
typedef struct {
    mulib_glx_window_t window;
    mulib_retgtk_scene_t scene;
    mulib_retgtk_event_router_t router;
    mgui_theme theme;
    const mulib_glx_font_t *font;
    mgx_surface retained;
} mulib_retgtk_window_t;
static inline int mulib_retgtk_window_init(mulib_retgtk_window_t *w,
        mulib_glx_platform_t *p,uint32_t id,const char *title,int width,int height)
{
    if (!w) return 0;
    memset(w,0,sizeof *w); w->theme=mgui_default_theme();
    return mulib_glx_window_init(&w->window,p,id,title,width,height);
}
static inline int mulib_retgtk_window_event(mulib_retgtk_window_t *w,
                                          const mgx_event *e)
{
    if (!w || !e) return 0;
    if (e->type==MGX_EVENT_CLOSE) { w->window.closed=1; return 1; }
    if (e->type==MGX_EVENT_RESIZE) {
        int ok=mulib_glx_window_resize(&w->window,mgx_int(e->x),mgx_int(e->y));
        if (ok) mulib_retgtk_scene_invalidate(&w->scene);
        return ok;
    }
    return mulib_retgtk_event_dispatch(&w->router,w->scene.root,e);
}
/* Retain one CPU image, then copy it to each acquired native swapchain image.
 * This avoids stale alternating buffers without assuming backend buffer age.
 * It reduces widget/layout work, not presentation bandwidth. */
static inline int mulib_retgtk_window_draw(mulib_retgtk_window_t *w)
{
    if (!w) return 0;
    mgx_surface *s=mulib_glx_swapchain_acquire(&w->window.swap);
    if (!s) return 0;
    if (!w->retained.pixels || w->retained.w!=s->w || w->retained.h!=s->h) {
        mgx_surface next={0};
        if (!mgx_surface_init(&next,s->w,s->h)) {
            mulib_glx_swapchain_cancel(&w->window.swap);
            return 0;
        }
        mgx_surface_free(&w->retained); w->retained=next;
        mulib_retgtk_scene_invalidate(&w->scene);
    }
    mret_paint p={&w->retained,w->font,w->theme};
    if (mulib_retgtk_scene_repaint(&w->scene,&p,(mgx_rect){0,0,s->w,s->h})<0) {
        mulib_glx_swapchain_cancel(&w->window.swap);
        return 0;
    }
    for (int y=0;y<s->h;y++)
        memcpy(s->pixels+(size_t)y*s->stride,
               w->retained.pixels+(size_t)y*w->retained.stride,
               (size_t)s->w*sizeof(uint32_t));
    if (!mulib_glx_window_present(&w->window)) {
        mulib_glx_swapchain_cancel(&w->window.swap);
        return 0;
    }
    return 1;
}
static inline void mulib_retgtk_window_free(mulib_retgtk_window_t *w)
{
    if (!w) return;
    mulib_retgtk_event_router_free(&w->router);
    mulib_retgtk_scene_set(&w->scene,NULL);
    mgx_surface_free(&w->retained);
    mulib_glx_window_free(&w->window);
}
#endif
