/* MuLIB graphical extension: glx-fontscan. C11, header-only. */
#ifndef MULIB_GLX_FONTSCAN_H
#define MULIB_GLX_FONTSCAN_H
#include "mulib-glx-core.h"
#include <dirent.h>
typedef int (*mgx_fontscan_fn)(void *,const char *);
static inline int mulib_glx_fontscan(const char *dir,mgx_fontscan_fn fn,void *u,unsigned depth){if(!dir||!fn||depth>16)return -1;DIR *d=opendir(dir);if(!d)return -1;int count=0;struct dirent *e;while((e=readdir(d))){if(e->d_name[0]=='.')continue;char path[4096];int n=snprintf(path,sizeof path,"%s/%s",dir,e->d_name);if(n<0||(size_t)n>=sizeof path)continue;const char *ext=strrchr(e->d_name,'.');if(ext&&(!strcmp(ext,".ttf")||!strcmp(ext,".otf")||!strcmp(ext,".psf")||!strcmp(ext,".ttc"))){count++;if(!fn(u,path))break;}else if(depth){int k=mulib_glx_fontscan(path,fn,u,depth-1);if(k>0)count+=k;}}closedir(d);return count;}
#endif
