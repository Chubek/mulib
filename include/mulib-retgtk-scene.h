/* MuLIB graphical extension: retgtk-scene. C11, header-only. */
#ifndef MULIB_RETGTK_SCENE_H
#define MULIB_RETGTK_SCENE_H
#include "mulib-retgtk-widget.h"
typedef struct{mulib_retgtk_widget_t *root;uint64_t generation;}mulib_retgtk_scene_t;
static inline void mulib_retgtk_scene_set(mulib_retgtk_scene_t *s,mulib_retgtk_widget_t *w){if(!s||(w&&!mulib_retgtk_object_ref(&w->object)))return;mulib_retgtk_widget_t *old=s->root;s->root=w;s->generation++;mulib_retgtk_widget_unref(old);}
static inline void mulib_retgtk_scene_draw(mulib_retgtk_scene_t *s,mret_paint *p,mgx_rect bounds){if(s->root){mulib_retgtk_widget_arrange(s->root,bounds);mulib_retgtk_widget_paint(s->root,p);}}
#endif
