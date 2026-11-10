/** @file mulib-imagejpeg.h
 * @brief Bounds-checked baseline JPEG marker inspection and image dimensions from SOF segments.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_IMAGEJPEG once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-imagejpeg.md for scope and limitations.
 */
#ifndef MULIB_IMAGEJPEG_H
#define MULIB_IMAGEJPEG_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_imagejpeg_dimensions(const void *jpeg,size_t bytes,uint16_t *width,uint16_t *height);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMAGEJPEG_H */
#if defined(MULIB_IMPL_IMAGEJPEG) && !defined(MULIB_IMAGEJPEG_H_IMPLEMENTED)
#define MULIB_IMAGEJPEG_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_imagejpeg_dimensions(const void *p,size_t n,uint16_t *w,uint16_t *h){if(!p||!w||!h||n<4)return 0;const unsigned char *s=(const unsigned char*)p;if(s[0]!=0xff||s[1]!=0xd8)return 0;size_t i=2;while(i<n){if(s[i++]!=0xff)return 0;while(i<n&&s[i]==0xff)i++;if(i>=n)return 0;unsigned m=s[i++];if(m==0xd9||m==0xda)return 0;if(m==0x01||(m>=0xd0&&m<=0xd7))continue;if(n-i<2)return 0;size_t len=((size_t)s[i]<<8)|s[i+1];if(len<2||len>n-i)return 0;if((m>=0xc0&&m<=0xcf)&&m!=0xc4&&m!=0xc8&&m!=0xcc){if(len<7)return 0;uint16_t hh=(uint16_t)((s[i+3]<<8)|s[i+4]),ww=(uint16_t)((s[i+5]<<8)|s[i+6]);if(!ww||!hh)return 0;*w=ww;*h=hh;return 1;}i+=len;}return 0;}
#ifdef __cplusplus
}
#endif
#endif
