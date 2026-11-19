/* MuLIB graphical extension: retgtk-inspector. C11, header-only. */
#ifndef MULIB_RETGTK_INSPECTOR_H
#define MULIB_RETGTK_INSPECTOR_H
#include "mulib-retgtk-box.h"
#include "mulib-retgtk-label.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_inspector(uint64_t id,const mulib_retgtk_widget_t *target){if(!target)return NULL;mulib_retgtk_widget_t *w=mulib_retgtk_box(id,1,2,2);if(!w)return NULL;char text[128];for(int i=0;i<4;i++){if(i==0)snprintf(text,sizeof text,"ID: %llu",(unsigned long long)target->object.id);else if(i==1)snprintf(text,sizeof text,"Bounds: %d,%d %dx%d",target->bounds.x,target->bounds.y,target->bounds.w,target->bounds.h);else if(i==2)snprintf(text,sizeof text,"Children: %zu",target->n);else snprintf(text,sizeof text,"Visible: %d Enabled: %d",target->visible,target->enabled);mulib_retgtk_widget_t *l=mulib_retgtk_label(id+i+1,text);if(!l||!mulib_retgtk_widget_add(w,l)){mulib_retgtk_widget_unref(l);mulib_retgtk_widget_unref(w);return NULL;}mulib_retgtk_widget_unref(l);}return w;}
#endif
