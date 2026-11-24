/* MuLIB graphical extension: immgtk-nodeeditor. C11, header-only. */
#ifndef MULIB_IMMGTK_NODEEDITOR_H
#define MULIB_IMMGTK_NODEEDITOR_H
#include "mulib-immgtk-native.h"
typedef struct{uint64_t id;mgx_rect box;const char *label;}mig_node;
typedef struct{uint64_t from,to;}mig_edge;
static inline int mulib_immgtk_nodeeditor(mulib_immgtk_native_t *g,mig_node *nodes,size_t n,const mig_edge *edges,size_t m,uint64_t *selected){if(!g||(!nodes&&n)||(!edges&&m))return 0;int changed=0;for(size_t i=0;i<m;i++){const mig_node *a=NULL,*b=NULL;for(size_t j=0;j<n;j++){if(nodes[j].id==edges[i].from)a=nodes+j;if(nodes[j].id==edges[i].to)b=nodes+j;}if(a&&b){mgx_drawcmd c={0};c.kind=MGX_LINE;c.clip=g->clip;c.a=(mgx_point){a->box.x+(double)a->box.w,a->box.y+(double)a->box.h/2};c.b=(mgx_point){b->box.x,b->box.y+(double)b->box.h/2};c.width=2;c.color=g->theme.accent;if(!mig_draw_add(g,c))return 0;}}for(size_t i=0;i<n;i++){mig_node *a=nodes+i;char key[32];snprintf(key,sizeof key,"node/%llu",(unsigned long long)a->id);uint64_t id=mig_id(g,key);if(mig_button(g,key,a->label,a->box,1)&&selected)*selected=a->id;mig_state *st=mig_state_get(g,id);if(st&&g->active==id&&g->down){if(g->pressed){st->rect.x=mgx_int(g->press_x-a->box.x);st->rect.y=mgx_int(g->press_y-a->box.y);}a->box.x=mgx_int(g->x-st->rect.x);a->box.y=mgx_int(g->y-st->rect.y);changed=1;}}return changed;}
#endif
