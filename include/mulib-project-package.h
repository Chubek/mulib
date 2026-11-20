/** @file mulib-project-package.h
 * Portable relative archive path validation and checked writer dispatch.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not an archive implementation or sufficient extraction security; handle symlinks, DOS reserved names and collisions.
 */
#ifndef MULIB_PROJECT_PACKAGE_H
#define MULIB_PROJECT_PACKAGE_H
#include "mulib-media-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* s borrows n bytes. ASCII case-insensitive DOS device stems, even with an
 * extension. Only path syntax screening, not extraction/symlink protection. */
static inline bool mulib_project_package_reserved(const char *s,size_t n){
 if(!s)return false;
 char stem[7];size_t k=0;
 while(k<n&&s[k]!='.')k++;
 while(k&&s[k-1]==' ')k--;
 if(k>sizeof stem)return false;
 for(size_t i=0;i<k;i++){unsigned char c=(unsigned char)s[i];stem[i]=(char)(c>='a'&&c<='z'?c-'a'+'A':c);}
 if(k==3&&(!memcmp(stem,"CON",3)||!memcmp(stem,"PRN",3)||!memcmp(stem,"AUX",3)||!memcmp(stem,"NUL",3)))return true;
 if(k==6&&!memcmp(stem,"CONIN$",6))return true;
 if(k==7&&!memcmp(stem,"CONOUT$",7))return true;
 if(k>=4&&(!memcmp(stem,"COM",3)||!memcmp(stem,"LPT",3))){
  if(k==4&&stem[3]>='1'&&stem[3]<='9')return true;
  /* Windows also treats UTF-8 superscript 1/2/3 as reserved device digits. */
  if(k==5&&(unsigned char)stem[3]==0xc2&&((unsigned char)stem[4]==0xb9||(unsigned char)stem[4]==0xb2||(unsigned char)stem[4]==0xb3))return true;
 }
 return false;
}
/* s borrows exactly n bytes (no terminator needed); embedded NUL is rejected.
 * Reject traversal, empty components, DOS separators/devices and trailing dots/
 * spaces. This does not normalize Unicode or detect filesystem name collisions. */
static inline bool mulib_project_package_path_n(const char *s,size_t n){
 if(!s||!n||s[0]=='/')return false;
 size_t start=0;
 for(size_t i=0;;i++){
  unsigned char c=i==n?0:(unsigned char)s[i];
  if(i==n||c=='/'){
   size_t len=i-start;
   if(!len||(len==1&&s[start]=='.')||(len==2&&s[start]=='.'&&s[start+1]=='.')||
      s[i-1]=='.'||s[i-1]==' '||mulib_project_package_reserved(s+start,len))return false;
   if(i==n)return true;
   start=i+1;
  }else if(c<32||c==127||c=='\\'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')return false;
 }
}
/* Compatibility wrapper: s must be a live NUL-terminated string. Use path_n
 * when validating untrusted bounded input. */
static inline bool mulib_project_package_path(const char *s){return s&&mulib_project_package_path_n(s,strlen(s));}
typedef bool (*mulib_project_package_visit)(void*,const char*,mulib_media_bytes);
static inline bool mulib_project_package_add(const char *path,mulib_media_bytes data,mulib_project_package_visit write,void *ctx){return write&&mulib_project_package_path(path)&&mulib_media_span_valid(data)&&write(ctx,path,data);}
#ifdef __cplusplus
}
#endif
#endif
