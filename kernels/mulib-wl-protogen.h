/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_PROTOGEN_H
#define MULIB_WL_PROTOGEN_H
#include "mulib-wm-core.h"
#include <stdio.h>
/* Descriptor-to-C generator. Feed parsed XML interface/message declarations from mulib-xml. */
typedef struct {const char *name,*signature;unsigned since;} mulib_wl_message_desc;
static inline int mulib_wl_protogen_identifier(const char *s){if(!s||!*s)return 0;for(size_t i=0;s[i];i++){unsigned c=(unsigned char)s[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||(i&&c>='0'&&c<='9')))return 0;}return 1;}
static inline int mulib_wl_protogen_emit(FILE *f,const char *iface,const mulib_wl_message_desc *v,size_t n){if(!f||!mulib_wl_protogen_identifier(iface)||(!v&&n)||n>65536)return 0;for(size_t i=0;i<n;i++){if(!mulib_wl_protogen_identifier(v[i].name)||!v[i].since||!v[i].signature)return 0;for(const char *s=v[i].signature;*s;s++)if(!strchr("iufsonah?",*s))return 0;for(size_t j=0;j<i;j++)if(!strcmp(v[i].name,v[j].name))return 0;}if(fprintf(f,"/* Generated protocol constants. */\n#ifndef MULIB_WL_GEN_%s\n#define MULIB_WL_GEN_%s\n",iface,iface)<0)return 0;for(size_t i=0;i<n;i++)if(fprintf(f,"#define %s_%s_opcode %zu\n#define %s_%s_since %u\n#define %s_%s_signature \"%s\"\n",iface,v[i].name,i,iface,v[i].name,v[i].since,iface,v[i].name,v[i].signature)<0)return 0;return fputs("#endif\n",f)!=EOF;}
#endif
