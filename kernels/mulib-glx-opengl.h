/* MuLIB graphical extension: glx-opengl. C11, header-only. */
#ifndef MULIB_GLX_OPENGL_H
#define MULIB_GLX_OPENGL_H
#include "mulib-glx-core.h"
#include <dlfcn.h>
typedef struct{void *lib;void (*clear_color)(float,float,float,float);void (*clear)(unsigned);}mulib_glx_opengl_t;
/* Requires current context supplied by caller. */
static inline int mulib_glx_opengl_open(mulib_glx_opengl_t *g){void *l=dlopen("libGL.so.1",RTLD_NOW|RTLD_LOCAL);if(!l)return 0;void *a=dlsym(l,"glClearColor"),*b=dlsym(l,"glClear");if(!a||!b||sizeof a!=sizeof g->clear_color||sizeof b!=sizeof g->clear){dlclose(l);return 0;}g->lib=l;memcpy(&g->clear_color,&a,sizeof a);memcpy(&g->clear,&b,sizeof b);return 1;}
static inline int mulib_glx_opengl_clear(mulib_glx_opengl_t *g,uint32_t c){if(!g||!g->lib)return 0;g->clear_color(mgx_ch(c,16)/255.f,mgx_ch(c,8)/255.f,mgx_ch(c,0)/255.f,mgx_ch(c,24)/255.f);g->clear(0x4000);return 1;}
static inline void mulib_glx_opengl_close(mulib_glx_opengl_t *g){if(g->lib)dlclose(g->lib);memset(g,0,sizeof *g);}
#endif
