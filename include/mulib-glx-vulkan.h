/* MuLIB graphical extension: glx-vulkan. C11, header-only. */
#ifndef MULIB_GLX_VULKAN_H
#define MULIB_GLX_VULKAN_H
#include "mulib-glx-core.h"
/* Optional runtime symbol resolution only. No Vulkan device creation. */
#include <dlfcn.h>
typedef struct{void *lib,*get_instance_proc_addr;}mulib_glx_vulkan_t;
static inline int mulib_glx_vulkan_open(mulib_glx_vulkan_t *v){void *lib=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);if(!lib)return 0;void *p=dlsym(lib,"vkGetInstanceProcAddr");if(!p){dlclose(lib);return 0;}*v=(mulib_glx_vulkan_t){lib,p};return 1;}
static inline void *mulib_glx_vulkan_symbol(const mulib_glx_vulkan_t *v,const char *name){return v&&v->lib?dlsym(v->lib,name):NULL;}
static inline void mulib_glx_vulkan_close(mulib_glx_vulkan_t *v){if(v->lib)dlclose(v->lib);memset(v,0,sizeof *v);}
#endif
