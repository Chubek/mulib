/* MuLIB: hwil-serialize. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_SERIALIZE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_SERIALIZE_H
#define MULIB_HWIL_SERIALIZE_H
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
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_serialize(const mulib_hwil_hw_context *c,mulib_storage_bytes *out);
int mulib_hwil_deserialize(const unsigned char *p,size_t n,mulib_hwil_hw_context *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_SERIALIZE) && !defined(MULIB_HWIL_SERIALIZE_H_IMPLEMENTED)
#define MULIB_HWIL_SERIALIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_serialize(const mulib_hwil_hw_context *c,mulib_storage_bytes *out){if(!c||!out||out->v)return 0;mulib_storage_bytes b={0};if(!mulib_storage_bytes_append(&b,"AHW1",4)||!mulib_storage_write_u64le(&b,c->n))goto fail;for(size_t i=0;i<c->n;i++){const mulib_hwil_hw_node *n=c->nodes+i;if(!mulib_hwil_hw_node_valid(c,i)||!mulib_storage_write_u64le(&b,n->op)||!mulib_storage_write_u64le(&b,n->width)||!mulib_storage_write_u64le(&b,n->argc)||!mulib_storage_write_u64le(&b,n->constant))goto fail;for(unsigned j=0;j<3;j++)if(!mulib_storage_write_u64le(&b,n->arg[j]))goto fail;}*out=b;return 1;fail:mulib_storage_bytes_free(&b);return 0;}
int mulib_hwil_deserialize(const unsigned char *p,size_t n,mulib_hwil_hw_context *out){if(!p||n<12||memcmp(p,"AHW1",4)||!out||out->nodes)return 0;size_t pos=4;uint64_t count;if(!mulib_storage_read_u64le(p,n,&pos,&count)||count!=(n-pos)/56||(n-pos)%56)return 0;mulib_hwil_hw_context c={0};if(!mulib_storage_reserve((void**)&c.nodes,&c.cap,(size_t)count,sizeof *c.nodes))return 0;c.n=(size_t)count;for(size_t i=0;i<c.n;i++){uint64_t op,w,argc,v;if(!mulib_storage_read_u64le(p,n,&pos,&op)||!mulib_storage_read_u64le(p,n,&pos,&w)||!mulib_storage_read_u64le(p,n,&pos,&argc)||!mulib_storage_read_u64le(p,n,&pos,&v)||op>MULIB_HWIL_HW_REG||w>64||argc>3)goto fail;c.nodes[i]=(mulib_hwil_hw_node){(unsigned)op,(unsigned)w,(unsigned)argc,{0},v};for(unsigned j=0;j<3;j++){if(!mulib_storage_read_u64le(p,n,&pos,&v)||v>SIZE_MAX)goto fail;c.nodes[i].arg[j]=(size_t)v;}}for(size_t i=0;i<c.n;i++)if(!mulib_hwil_hw_node_valid(&c,i))goto fail;*out=c;return 1;fail:mulib_hwil_context_free(&c);return 0;}
#ifdef __cplusplus
}
#endif
#endif
