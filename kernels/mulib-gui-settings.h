/* MuLIB graphical extension: gui-settings. C11, header-only. */
#ifndef MULIB_GUI_SETTINGS_H
#define MULIB_GUI_SETTINGS_H
#include "mulib-gui-core.h"
typedef struct{char key[64],value[256];}mgui_setting;
typedef struct{mgui_setting *v;size_t n,cap;}mulib_gui_settings_t;
#ifndef MULIB_GUI_SETTINGS_LIMIT
#define MULIB_GUI_SETTINGS_LIMIT 4096u
#endif
#ifndef MULIB_GUI_SETTINGS_INPUT_LIMIT
#define MULIB_GUI_SETTINGS_INPUT_LIMIT (1024u*1024u)
#endif
static inline int mulib_gui_settings_set(mulib_gui_settings_t *s,const char *k,const char *v){
 size_t kn,vn;
 if(!s||!mgui_string_length(k,64,&kn)||!kn||!mgui_string_length(v,256,&vn)||
    strpbrk(k,"=\r\n")||strpbrk(v,"\r\n"))return 0;
 /* Copy before growth, including when arguments borrow this store's strings. */
 mgui_setting t={0};memcpy(t.key,k,kn+1);memcpy(t.value,v,vn+1);
 size_t i=0;while(i<s->n&&strcmp(s->v[i].key,t.key))i++;
 if(i==s->n){
  if(s->n>=MULIB_GUI_SETTINGS_LIMIT||s->n==SIZE_MAX||
     !mgx_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v))return 0;
  s->n++;
 }
 s->v[i]=t;return 1;
}
static inline const char *mulib_gui_settings_get(const mulib_gui_settings_t *s,const char *k){
 if(!s||!mgui_string_length(k,64,NULL))return NULL;
 for(size_t i=0;i<s->n;i++)if(!strcmp(s->v[i].key,k))return s->v[i].value;
 return NULL;
}
static inline int mulib_gui_settings_write(const mulib_gui_settings_t *s,FILE *f){
 if(!s||!f)return 0;
 for(size_t i=0;i<s->n;i++)if(fprintf(f,"%s=%s\n",s->v[i].key,s->v[i].value)<0)return 0;
 return !ferror(f);
}
/* Transactional merge; duplicate keys use the last value. On failure the store
 * is unchanged, but the caller-owned stream has consumed an unspecified prefix. */
static inline int mulib_gui_settings_read(mulib_gui_settings_t *s,FILE *f){
 if(!s||!f||s->n>MULIB_GUI_SETTINGS_LIMIT)return 0;
 mulib_gui_settings_t t={0};
 if(s->n){
  if(!mgx_grow((void**)&t.v,&t.cap,s->n,sizeof *t.v))return 0;
  memcpy(t.v,s->v,s->n*sizeof *t.v);t.n=s->n;
 }
 char line[322];size_t budget=MULIB_GUI_SETTINGS_INPUT_LIMIT;int status;
 while((status=mgui_read_line(f,line,sizeof line,&budget))==1){
  char *p=strchr(line,'=');
  if(!p){free(t.v);return 0;}
  *p++=0;
  if(!mulib_gui_settings_set(&t,line,p)){free(t.v);return 0;}
 }
 if(status<0){free(t.v);return 0;}
 free(s->v);*s=t;return 1;
}
static inline void mulib_gui_settings_free(mulib_gui_settings_t *s){if(s){free(s->v);memset(s,0,sizeof *s);}}
#endif
