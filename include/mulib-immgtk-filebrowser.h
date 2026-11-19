/* MuLIB graphical extension: immgtk-filebrowser. C11, header-only. */
#ifndef MULIB_IMMGTK_FILEBROWSER_H
#define MULIB_IMMGTK_FILEBROWSER_H
#include "mulib-immgtk-native.h"
#include "mulib-gui-filedialog.h"
static inline int mulib_immgtk_filebrowser(mulib_immgtk_native_t *g,const mulib_gui_filedialog_t *f,mgx_rect r,size_t first,size_t *choice){for(size_t i=first;i<f->n&&i-first<(size_t)mgx_max(0,r.h/24);i++){char key[40];snprintf(key,sizeof key,"file/%zu",i);if(mig_button(g,key,f->v[i].name,(mgx_rect){r.x,r.y+(int)(i-first)*24,r.w,24},1)){*choice=i;return 1;}}return 0;}
#endif
