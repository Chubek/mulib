/* MuLIB graphical extension: gui-portal. C11, header-only. */
#ifndef MULIB_GUI_PORTAL_H
#define MULIB_GUI_PORTAL_H
#include "mulib-gui-core.h"
/* XDG portal request bookkeeping; actual D-Bus transport supplied by host. */
typedef struct{uint64_t serial;char token[64],path[256];int pending,result;}mulib_gui_portal_t;
static inline int mulib_gui_portal_begin(mulib_gui_portal_t *p,const char *sender){if(p->pending||!sender||strlen(sender)>120||p->serial==UINT64_MAX)return 0;char s[128];size_t k=0;for(;*sender;sender++){unsigned c=(unsigned char)*sender;if(c==':'&&!k)continue;if(c=='.')c='_';if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'))return 0;s[k++]=(char)c;}if(!k)return 0;s[k]=0;snprintf(p->token,sizeof p->token,"mulib_%llu",(unsigned long long)++p->serial);snprintf(p->path,sizeof p->path,"/org/freedesktop/portal/desktop/request/%s/%s",s,p->token);p->pending=1;return 1;}
static inline int mulib_gui_portal_response(mulib_gui_portal_t *p,const char *path,unsigned result){if(!p->pending||!path||strcmp(path,p->path)||result>2)return 0;p->pending=0;p->result=result;return 1;}
#endif
