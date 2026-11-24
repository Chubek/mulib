/* MuLIB graphical extension: gui-mime. C11, header-only. */
#ifndef MULIB_GUI_MIME_H
#define MULIB_GUI_MIME_H
#include "mulib-gui-core.h"
static inline int mgui_mime_token(unsigned char c){
 return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
        (c&&strchr("!#$%&'*+-.^_`|~",c)!=NULL);
}
/* Bounded ASCII media type, optionally followed by token-valued parameters.
 * Quoted parameters and whitespace other than a space after ';' are unsupported. */
static inline int mulib_gui_mime_valid(const char *mime){
 size_t n=0,i=0,start;
 if(!mgui_string_length(mime,128,&n)||!n)return 0;
 while(i<n&&mgui_mime_token((unsigned char)mime[i])){if(mime[i]=='*')return 0;i++;}
 if(!i||i==n||mime[i++]!='/')return 0;
 start=i;while(i<n&&mgui_mime_token((unsigned char)mime[i])){if(mime[i]=='*')return 0;i++;}
 if(i==start)return 0;
 while(i<n){
  if(mime[i++]!=';')return 0;
  while(i<n&&mime[i]==' ')i++;
  start=i;while(i<n&&mgui_mime_token((unsigned char)mime[i]))i++;
  if(i==start||i==n||mime[i++]!='=')return 0;
  start=i;while(i<n&&mgui_mime_token((unsigned char)mime[i]))i++;
  if(i==start)return 0;
 }
 return 1;
}
static inline int mgui_extension_equal(const char *a,const char *b){
 while(*a&&*b){
  unsigned char c=(unsigned char)*a++;
  if(c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');
  if(c!=(unsigned char)*b++)return 0;
 }
 return !*a&&!*b;
}
/* Small heuristic, not shared-mime-info or content security validation. */
static inline const char *mulib_gui_mime_guess(const char *name,const void *data,size_t n){
 const unsigned char *p=data;
 if(p&&n>=8&&!memcmp(p,"\x89PNG\r\n\x1a\n",8))return "image/png";
 if(p&&n>=3&&p[0]==255&&p[1]==216&&p[2]==255)return "image/jpeg";
 if(p&&n>=5&&!memcmp(p,"%PDF-",5))return "application/pdf";
 if(p&&n>=12&&!memcmp(p,"RIFF",4)&&!memcmp(p+8,"WEBP",4))return "image/webp";
 if(p&&n>=6&&(!memcmp(p,"GIF87a",6)||!memcmp(p,"GIF89a",6)))return "image/gif";
 const char *e=NULL;
 if(name)for(const char *s=name;*s;s++){if(*s=='/'||*s=='\\')e=NULL;else if(*s=='.')e=s;}
 if(e){
  if(mgui_extension_equal(e,".svg"))return "image/svg+xml";
  if(mgui_extension_equal(e,".json"))return "application/json";
  if(mgui_extension_equal(e,".txt")||mgui_extension_equal(e,".md"))return "text/plain";
  if(mgui_extension_equal(e,".html")||mgui_extension_equal(e,".htm"))return "text/html";
  if(mgui_extension_equal(e,".png"))return "image/png";
  if(mgui_extension_equal(e,".jpg")||mgui_extension_equal(e,".jpeg"))return "image/jpeg";
  if(mgui_extension_equal(e,".webp"))return "image/webp";
 }
 return "application/octet-stream";
}
#endif
