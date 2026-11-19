/* MuLIB graphical extension: retgtk-window. C11, header-only. */
#ifndef MULIB_RETGTK_WINDOW_H
#define MULIB_RETGTK_WINDOW_H
#include "mulib-retgtk-scene.h"
#include "mulib-retgtk-event.h"
#include "mulib-glx-window.h"
typedef struct{mulib_glx_window_t window;mulib_retgtk_scene_t scene;mulib_retgtk_event_router_t router;mgui_theme theme;const mulib_glx_font_t *font;}mulib_retgtk_window_t;
static inline int mulib_retgtk_window_init(mulib_retgtk_window_t *w,mulib_glx_platform_t *p,uint32_t id,const char *title,int width,int height){memset(w,0,sizeof *w);w->theme=mgui_default_theme();return mulib_glx_window_init(&w->window,p,id,title,width,height);}
static inline int mulib_retgtk_window_event(mulib_retgtk_window_t *w,const mgx_event *e){if(e->type==MGX_EVENT_CLOSE){w->window.closed=1;return 1;}if(e->type==MGX_EVENT_RESIZE){int ok=mulib_glx_window_resize(&w->window,mgx_int(e->x),mgx_int(e->y));if(ok&&w->scene.root)mulib_retgtk_widget_invalidate(w->scene.root);return ok;}return mulib_retgtk_event_dispatch(&w->router,w->scene.root,e);}
static inline int mulib_retgtk_window_draw(mulib_retgtk_window_t *w){mgx_surface *s=mulib_glx_swapchain_acquire(&w->window.swap);if(!s)return 0;mgx_clear(s,w->theme.background);mret_paint p={s,w->font,w->theme};mulib_retgtk_scene_draw(&w->scene,&p,(mgx_rect){0,0,s->w,s->h});if(!mulib_glx_window_present(&w->window)){w->window.swap.acquired=0;return 0;}return 1;}
static inline void mulib_retgtk_window_free(mulib_retgtk_window_t *w){mulib_retgtk_event_router_free(&w->router);mulib_retgtk_scene_set(&w->scene,NULL);mulib_glx_window_free(&w->window);}
#endif
