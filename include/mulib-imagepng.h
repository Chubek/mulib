/** @file mulib-imagepng.h
 * @brief Encode 8-bit RGB PNG using uncompressed DEFLATE blocks and CRC-checked chunks.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_IMAGEPNG once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-imagepng.md for scope and limitations.
 */
#ifndef MULIB_IMAGEPNG_H
#define MULIB_IMAGEPNG_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_imagepng_encode_rgb(const unsigned char *pixels,uint32_t width,uint32_t height,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMAGEPNG_H */
#if defined(MULIB_IMPL_IMAGEPNG) && !defined(MULIB_IMAGEPNG_H_IMPLEMENTED)
#define MULIB_IMAGEPNG_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_imagepng_chunk_(mulib_typecore_buf_t *b,const char tag[4],const void *p,size_t n){if(n>UINT32_MAX)return 0;mulib_typecore_buf_t t={0};int ok=mulib_typecore_append(&t,tag,4)&&mulib_typecore_append(&t,p,n)&&mulib_typecore_u32be(b,(uint32_t)n)&&mulib_typecore_append(b,t.data,t.size)&&mulib_typecore_u32be(b,mulib_typecore_crc32(t.data,t.size));mulib_typecore_free(&t);return ok;}
int mulib_imagepng_encode_rgb(const unsigned char *src,uint32_t width,uint32_t height,mulib_typecore_buf_t *out){if(!src||!out||!width||!height||width>100000||height>100000 )return 0;size_t stride=(size_t)width*3;if(height>SIZE_MAX/(stride+1))return 0;size_t rawlen=(stride+1)*(size_t)height;if(rawlen>128*1024*1024)return 0;
 mulib_typecore_buf_t raw={0},z={0},b={0};const unsigned char sig[]={137,80,78,71,13,10,26,10};unsigned char ihdr[13]={0};ihdr[0]=(unsigned char)(width>>24);ihdr[1]=(unsigned char)(width>>16);ihdr[2]=(unsigned char)(width>>8);ihdr[3]=(unsigned char)width;ihdr[4]=(unsigned char)(height>>24);ihdr[5]=(unsigned char)(height>>16);ihdr[6]=(unsigned char)(height>>8);ihdr[7]=(unsigned char)height;ihdr[8]=8;ihdr[9]=2;
 for(uint32_t row=0;row<height;row++){if(!mulib_typecore_byte(&raw,0)||!mulib_typecore_append(&raw,src+(size_t)row*stride,stride))goto fail;}
 if(!mulib_typecore_byte(&z,0x78)||!mulib_typecore_byte(&z,0x01))goto fail;
 for(size_t i=0;i<raw.size;){size_t n=raw.size-i;if(n>65535)n=65535;uint16_t k=(uint16_t)n;unsigned char head[5]={(unsigned char)(i+n==raw.size),(unsigned char)k,(unsigned char)(k>>8),(unsigned char)(~k),(unsigned char)((~k)>>8)};if(!mulib_typecore_append(&z,head,5)||!mulib_typecore_append(&z,raw.data+i,n))goto fail;i+=n;}
 if(!mulib_typecore_u32be(&z,mulib_typecore_adler32(raw.data,raw.size))||!mulib_typecore_append(&b,sig,8)||!mulib_imagepng_chunk_(&b,"IHDR",ihdr,13)||!mulib_imagepng_chunk_(&b,"IDAT",z.data,z.size)||!mulib_imagepng_chunk_(&b,"IEND",NULL,0)||!mulib_typecore_append(out,b.data,b.size))goto fail;
 mulib_typecore_free(&raw);mulib_typecore_free(&z);mulib_typecore_free(&b);return 1;
 fail:mulib_typecore_free(&raw);mulib_typecore_free(&z);mulib_typecore_free(&b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
