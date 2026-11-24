/* MuLIB graphical extension: immgtk-settings. C11, header-only. */
#ifndef MULIB_IMMGTK_SETTINGS_H
#define MULIB_IMMGTK_SETTINGS_H
#include "mulib-gui-settings.h"
static inline int mulib_immgtk_settings_store(mulib_gui_settings_t *s,const char *id,mgx_rect r){char v[96];snprintf(v,sizeof v,"%d %d %d %d",r.x,r.y,r.w,r.h);return mulib_gui_settings_set(s,id,v);}
static inline int mulib_immgtk_settings_load(const mulib_gui_settings_t *s,const char *id,mgx_rect *r){const char *v=mulib_gui_settings_get(s,id);mgx_rect t;if(!v||sscanf(v,"%d %d %d %d",&t.x,&t.y,&t.w,&t.h)!=4||t.w<=0||t.h<=0)return 0;*r=t;return 1;}
#endif
