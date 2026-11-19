/* MuLIB / Amb3r: ecad-kicad. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_KICAD in exactly one implementation translation unit.
 * Profile: S-expression lexical scanning and structural balance only; not complete KiCad schematic import/export.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_KICAD_H
#define MULIB_ECAD_KICAD_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*ma_kicad_token_cb)(void *,unsigned,const char *,size_t);
int mulib_ecad_kicad_scan(const char *text,size_t length,ma_kicad_token_cb cb,void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_KICAD) && !defined(MULIB_ECAD_KICAD_H_IMPLEMENTED)
#define MULIB_ECAD_KICAD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_kicad_scan(const char *text,size_t length,ma_kicad_token_cb cb,void *ctx){if((!text&&length)||!cb)return 0;size_t i=0,depth=0;while(i<length){unsigned char c=(unsigned char)text[i];if(isspace(c)){i++;continue;}size_t j=i;unsigned kind=0;if(c=='('){kind=1;depth++;i++;}else if(c==')'){if(!depth)return 0;depth--;kind=2;i++;}else if(c=='"'){kind=3;i++;int closed=0;while(i<length){if(text[i]=='\\'){if(++i==length)return 0;i++;}else if(text[i++]=='"'){closed=1;break;}}if(!closed)return 0;}else{kind=4;while(i<length&&!isspace((unsigned char)text[i])&&text[i]!='('&&text[i]!=')'&&text[i]!='"')i++;}if(!cb(ctx,kind,text+j,i-j))return 0;}return depth==0;}
#ifdef __cplusplus
}
#endif
#endif
