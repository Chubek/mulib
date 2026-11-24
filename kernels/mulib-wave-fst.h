/* MuLIB: wave-fst. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_FST in exactly one implementation translation unit.
 * Profile: LEB128 and timestamp-delta codec only; NOT an FST file reader/writer.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_FST_H
#define MULIB_WAVE_FST_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_fst_varint_write(mulib_storage_bytes *b,uint64_t value);
int mulib_wave_fst_varint_read(const unsigned char *p,size_t n,size_t *offset,uint64_t *out);
int mulib_wave_fst_deltas(mulib_storage_bytes *b,const uint64_t *times,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_FST) && !defined(MULIB_WAVE_FST_H_IMPLEMENTED)
#define MULIB_WAVE_FST_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_fst_varint_write(mulib_storage_bytes *b,uint64_t value){if(!b)return 0;unsigned char v[10];size_t n=0;do{v[n]=(unsigned char)(value&127);value>>=7;if(value)v[n]|=128;n++;}while(value);return mulib_storage_bytes_append(b,v,n);}
int mulib_wave_fst_varint_read(const unsigned char *p,size_t n,size_t *offset,uint64_t *out){if(!p||!offset||!out||*offset>n)return 0;size_t pos=*offset;uint64_t v=0;for(unsigned k=0;k<10;k++){if(pos==n)return 0;unsigned char x=p[pos++];if(k==9&&(x&254))return 0;v|=(uint64_t)(x&127)<<(7*k);if(!(x&128)){*offset=pos;*out=v;return 1;}}return 0;}
int mulib_wave_fst_deltas(mulib_storage_bytes *b,const uint64_t *times,size_t n){if(!b||(!times&&n))return 0;size_t old=b->n;uint64_t previous=0;for(size_t i=0;i<n;i++){if(times[i]<previous||!mulib_wave_fst_varint_write(b,times[i]-previous)){b->n=old;return 0;}previous=times[i];}return 1;}
#ifdef __cplusplus
}
#endif
#endif
