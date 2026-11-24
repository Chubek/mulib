/* MuLIB graphical extension: gui-app. C11, header-only. */
#ifndef MULIB_GUI_APP_H
#define MULIB_GUI_APP_H
#include "mulib-gui-dispatch.h"
#include "mulib-glx-platform.h"
typedef struct{mulib_glx_platform_t platform;mulib_glx_event_queue_t events;mulib_gui_dispatch_t dispatch;int quit;}mulib_gui_app_t;
static inline int mulib_gui_app_poll(mulib_gui_app_t *a,int timeout){if(a->quit)return 0;mulib_gui_dispatch_run(&a->dispatch,1024);if(a->platform.ops.poll&&a->platform.ops.poll(a->platform.user,&a->events,timeout)<0)return -1;return 1;}
static inline void mulib_gui_app_quit(mulib_gui_app_t *a){a->quit=1;}
static inline void mulib_gui_app_free(mulib_gui_app_t *a){mulib_gui_dispatch_free(&a->dispatch);mulib_glx_event_free(&a->events);mulib_glx_platform_close(&a->platform);}
#endif
