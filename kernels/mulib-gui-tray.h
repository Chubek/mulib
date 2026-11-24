/* MuLIB graphical extension: gui-tray. C11, header-only. */
#ifndef MULIB_GUI_TRAY_H
#define MULIB_GUI_TRAY_H
#include "mulib-gui-core.h"
typedef struct{char id[64],title[128],icon[128];int status;void (*activate)(void *,int,int);void *user;}mulib_gui_tray_t;
static inline int mulib_gui_tray_init(mulib_gui_tray_t *t,const char *id,const char *title,const char *icon){if(!id||!*id||strlen(id)>=64||!title||!icon)return 0;memset(t,0,sizeof *t);strcpy(t->id,id);snprintf(t->title,sizeof t->title,"%s",title);snprintf(t->icon,sizeof t->icon,"%s",icon);t->status=1;return 1;}
static inline int mulib_gui_tray_activate(mulib_gui_tray_t *t,int x,int y){if(!t->status||!t->activate)return 0;t->activate(t->user,x,y);return 1;}
#endif
