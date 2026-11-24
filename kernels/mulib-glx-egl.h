/* MuLIB graphical extension: glx-egl. C11, header-only. */
#ifndef MULIB_GLX_EGL_H
#define MULIB_GLX_EGL_H
#include "mulib-glx-core.h"
#include <dlfcn.h>
typedef struct{void *lib;void *(*get_display)(void *);unsigned (*initialize)(void *,int *,int *);unsigned (*terminate)(void *);void *display;}mulib_glx_egl_t;
static inline int mulib_glx_egl_open(mulib_glx_egl_t *e,void *native,int *major,int *minor){mulib_glx_egl_t t={0};t.lib=dlopen("libEGL.so.1",RTLD_NOW|RTLD_LOCAL);if(!t.lib)return 0;void *p=dlsym(t.lib,"eglGetDisplay"),*q=dlsym(t.lib,"eglInitialize"),*r=dlsym(t.lib,"eglTerminate");if(sizeof p!=sizeof t.get_display||sizeof q!=sizeof t.initialize||sizeof r!=sizeof t.terminate||!p||!q||!r){dlclose(t.lib);return 0;}memcpy(&t.get_display,&p,sizeof p);memcpy(&t.initialize,&q,sizeof q);memcpy(&t.terminate,&r,sizeof r);t.display=t.get_display(native);if(!t.display||!t.initialize(t.display,major,minor)){dlclose(t.lib);return 0;}*e=t;return 1;}
static inline void mulib_glx_egl_close(mulib_glx_egl_t *e){if(e->display)e->terminate(e->display);if(e->lib)dlclose(e->lib);memset(e,0,sizeof *e);}
#endif
