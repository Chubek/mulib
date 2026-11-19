/* MuLIB graphical extension: gui-filedialog. C11, header-only. */
#ifndef MULIB_GUI_FILEDIALOG_H
#define MULIB_GUI_FILEDIALOG_H
#include "mulib-gui-core.h"
#include <dirent.h>
#include <sys/stat.h>
typedef struct{char name[256];int directory;uint64_t size;}mgui_file_entry;
typedef struct{mgui_file_entry *v;size_t n,cap;}mulib_gui_filedialog_t;
static inline int mgui_file_compare(const void *a,const void *b){const mgui_file_entry *x=a,*y=b;return x->directory!=y->directory?y->directory-x->directory:strcmp(x->name,y->name);}
static inline int mulib_gui_filedialog_scan(mulib_gui_filedialog_t *f,const char *path,int hidden){DIR *d=opendir(path);if(!d)return 0;mulib_gui_filedialog_t t={0};struct dirent *e;while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||(!hidden&&e->d_name[0]=='.'))continue;char p[4096];int k=snprintf(p,sizeof p,"%s/%s",path,e->d_name);struct stat st;if(k<0||(size_t)k>=sizeof p||strlen(e->d_name)>=256||stat(p,&st)<0)continue;if(!mgx_grow((void**)&t.v,&t.cap,t.n+1,sizeof *t.v)){free(t.v);closedir(d);return 0;}mgui_file_entry *a=t.v+t.n++;strcpy(a->name,e->d_name);a->directory=S_ISDIR(st.st_mode);a->size=st.st_size<0?0:(uint64_t)st.st_size;}closedir(d);if(t.n)qsort(t.v,t.n,sizeof *t.v,mgui_file_compare);free(f->v);*f=t;return 1;}
static inline void mulib_gui_filedialog_free(mulib_gui_filedialog_t *f){free(f->v);memset(f,0,sizeof *f);}
#endif
