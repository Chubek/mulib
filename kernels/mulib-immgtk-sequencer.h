/* MuLIB graphical extension: immgtk-sequencer. C11, header-only. */
#ifndef MULIB_IMMGTK_SEQUENCER_H
#define MULIB_IMMGTK_SEQUENCER_H
#include "mulib-immgtk-timeline.h"
typedef struct{uint64_t id;double start,end;unsigned track;}mig_sequence_item;
static inline int mulib_immgtk_sequencer(mulib_immgtk_native_t *g,const mig_timeline *t,mig_sequence_item *v,size_t n,int track_h){int changed=0;if(track_h<=0)return 0;for(size_t i=0;i<n;i++){double a=mulib_immgtk_timeline_x(t,v[i].start),b=mulib_immgtk_timeline_x(t,v[i].end);mgx_rect r={mgx_int(a),mgx_int(t->bounds.y+(double)v[i].track*track_h),mgx_max(1,mgx_int(b-a)),track_h-1};char key[32];snprintf(key,sizeof key,"seq/%llu",(unsigned long long)v[i].id);uint64_t k=mig_id(g,key);mig_interact(g,k,r,1);mig_rect(g,r,g->theme.accent);mig_state *s=mig_state_get(g,k);if(s&&g->active==k&&g->down){double now=mulib_immgtk_timeline_time(t,g->x);if(g->pressed)s->value=now-v[i].start;double length=v[i].end-v[i].start,next=mgx_clamp(now-s->value,t->start,fmax(t->start,t->end-length));v[i].start=next;v[i].end=next+length;changed=1;}}return changed;}
#endif
