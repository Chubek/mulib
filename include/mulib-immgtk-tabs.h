/* MuLIB graphical extension: immgtk-tabs. C11, header-only. */
#ifndef MULIB_IMMGTK_TABS_H
#define MULIB_IMMGTK_TABS_H
#include "mulib-immgtk-native.h"
static inline int mulib_immgtk_tabs(mulib_immgtk_native_t *g,const char *const *labels,size_t n,size_t *selected,mgx_rect r){if(!n||!selected||n>(size_t)INT_MAX)return 0;int changed=0;for(size_t i=0;i<n;i++){char key[32];snprintf(key,sizeof key,"tab/%zu",i);int a=(int)((int64_t)r.w*i/n),b=(int)((int64_t)r.w*(i+1)/n);mgx_rect q={r.x+a,r.y,b-a,r.h};if(mig_button(g,key,labels[i],q,1)){*selected=i;changed=1;}if(*selected==i)mig_rect(g,(mgx_rect){q.x,q.y+q.h-2,q.w,2},g->theme.accent);}return changed;}
#endif
