/* MuLIB graphical extension: gui-locale. C11, header-only. */
#ifndef MULIB_GUI_LOCALE_H
#define MULIB_GUI_LOCALE_H
#include "mulib-gui-core.h"
typedef struct{const char *key,*value;}mgui_translation;
static inline const char *mulib_gui_locale_translate(const mgui_translation *v,size_t n,const char *key){if(!key)return "";for(size_t i=0;i<n;i++)if(!strcmp(v[i].key,key))return v[i].value;return key;}
static inline int mulib_gui_locale_rtl(const char *s){if(!s)return 0;return (!strncmp(s,"ar",2)||!strncmp(s,"fa",2)||!strncmp(s,"he",2)||!strncmp(s,"ur",2))&&(s[2]==0||s[2]=='-'||s[2]=='_');}
#endif
