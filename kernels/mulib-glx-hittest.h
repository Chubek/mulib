/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_GLX_HITTEST_H
#define MULIB_GLX_HITTEST_H
#include "mulib-glx-scene.h"
typedef struct {double x,y,local_x,local_y;uint64_t id;} mulib_glx_hit;
static inline int mulib_glx_hit_visit(void *u,const mulib_glx_scenenode *n,const mulib_glx_scene_world *w){mulib_glx_hit *h=(mulib_glx_hit*)u;if(w->visible&&w->opacity>0&&n->accept_input&&n->buffer&&h->x>=w->l&&h->x<w->r&&h->y>=w->t&&h->y<w->b){double x=(h->x-w->x)/w->sx,y=(h->y-w->y)/w->sy;if(x>=0&&y>=0&&x<n->buffer->w&&y<n->buffer->h){h->id=n->id;h->local_x=x;h->local_y=y;}}return 1;}
static inline uint64_t mulib_glx_hittest(mulib_glx_scene *s,double x,double y,mulib_glx_hit *out){mulib_glx_hit h={.x=x,.y=y};if(!isfinite(x)||!isfinite(y)||!mulib_glx_scene_walk(s,mulib_glx_hit_visit,&h))return 0;if(out)*out=h;return h.id;}
#endif
