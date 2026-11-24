/* MuLIB graphical extension: gui-desktop. C11, header-only. */
#ifndef MULIB_GUI_DESKTOP_H
#define MULIB_GUI_DESKTOP_H
#include "mulib-gui-core.h"
typedef struct{char name[256],exec[512],icon[256],type[64];int hidden,terminal;}mulib_gui_desktop_entry_t;
#ifndef MULIB_GUI_DESKTOP_INPUT_LIMIT
#define MULIB_GUI_DESKTOP_INPUT_LIMIT (64u*1024u)
#endif
static inline int mgui_desktop_string(char *out,size_t capacity,const char *s){
 size_t n=0;
 while(*s){
  unsigned char c=(unsigned char)*s++;
  if(c=='\\'){
   c=(unsigned char)*s++;
   if(c=='s')c=' ';else if(c=='n')c='\n';else if(c=='t')c='\t';
   else if(c=='r')c='\r';else if(c!='\\')return 0;
  }else if(c<32||c==127)return 0;
  if(n>=capacity-1)return 0;
  out[n++]=(char)c;
 }
 out[n]=0;return 1;
}
/* Bounded local metadata subset, NOT an application launcher or full desktop
 * entry implementation. Name/Icon/Type decode standard string escapes.
 * Exec remains untrusted, verbatim data: no tokenization, field-code expansion,
 * shell evaluation, subprocess, or environment expansion is performed here.
 * Localized and unknown keys are ignored. Duplicate known keys are rejected. */
static inline int mulib_gui_desktop_parse(mulib_gui_desktop_entry_t *d,FILE *f){
 if(!d||!f)return 0;
 mulib_gui_desktop_entry_t t={0};
 char line[1024];int section=0,seen_section=0,status;unsigned seen=0;
 size_t budget=MULIB_GUI_DESKTOP_INPUT_LIMIT;
 while((status=mgui_read_line(f,line,sizeof line,&budget))==1){
  if(!line[0]||line[0]=='#')continue;
  if(line[0]=='['){
   size_t n=strlen(line);if(n<2||line[n-1]!=']')return 0;
   section=!strcmp(line,"[Desktop Entry]");
   if(section&&seen_section++)return 0;
   continue;
  }
  if(!section)continue;
  char *p=strchr(line,'=');if(!p||p==line)return 0;*p++=0;
  unsigned bit=0;char *out=NULL;size_t capacity=0;
  if(!strcmp(line,"Name")){bit=1;out=t.name;capacity=sizeof t.name;}
  else if(!strcmp(line,"Exec")){bit=2;out=t.exec;capacity=sizeof t.exec;}
  else if(!strcmp(line,"Icon")){bit=4;out=t.icon;capacity=sizeof t.icon;}
  else if(!strcmp(line,"Type")){bit=8;out=t.type;capacity=sizeof t.type;}
  else if(!strcmp(line,"Hidden"))bit=16;
  else if(!strcmp(line,"Terminal"))bit=32;
  if(!bit)continue;
  if(seen&bit)return 0;
  seen|=bit;
  if(bit==2){
   size_t n;if(!mgui_string_length(p,capacity,&n)||strpbrk(p,"\t\r"))return 0;
   memcpy(out,p,n+1);
  }else if(out){if(!mgui_desktop_string(out,capacity,p))return 0;}
  else{
   if(strcmp(p,"true")&&strcmp(p,"false"))return 0;
   if(bit==16)t.hidden=!strcmp(p,"true");else t.terminal=!strcmp(p,"true");
  }
 }
 if(status<0||!t.name[0]||!t.type[0])return 0;
 *d=t;return 1;
}
#endif
