/* MuLIB / Amb3r: hwil-serialize. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_SERIALIZE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_SERIALIZE_H
#define MULIB_HWIL_SERIALIZE_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_serialize(const ma_hw_context *c,ma_bytes *out);
int mulib_hwil_deserialize(const unsigned char *p,size_t n,ma_hw_context *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_SERIALIZE) && !defined(MULIB_HWIL_SERIALIZE_H_IMPLEMENTED)
#define MULIB_HWIL_SERIALIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_serialize(const ma_hw_context *c,ma_bytes *out){if(!c||!out||out->v)return 0;ma_bytes b={0};if(!ma_bytes_append(&b,"AHW1",4)||!ma_write_u64(&b,c->n))goto fail;for(size_t i=0;i<c->n;i++){const ma_hw_node *n=c->nodes+i;if(!ma_hw_node_valid(c,i)||!ma_write_u64(&b,n->op)||!ma_write_u64(&b,n->width)||!ma_write_u64(&b,n->argc)||!ma_write_u64(&b,n->constant))goto fail;for(unsigned j=0;j<3;j++)if(!ma_write_u64(&b,n->arg[j]))goto fail;}*out=b;return 1;fail:ma_bytes_free(&b);return 0;}
int mulib_hwil_deserialize(const unsigned char *p,size_t n,ma_hw_context *out){if(!p||n<12||memcmp(p,"AHW1",4)||!out||out->nodes)return 0;size_t pos=4;uint64_t count;if(!ma_read_u64(p,n,&pos,&count)||count!=(n-pos)/56||(n-pos)%56)return 0;ma_hw_context c={0};if(!ma_reserve((void**)&c.nodes,&c.cap,(size_t)count,sizeof *c.nodes))return 0;c.n=(size_t)count;for(size_t i=0;i<c.n;i++){uint64_t op,w,argc,v;if(!ma_read_u64(p,n,&pos,&op)||!ma_read_u64(p,n,&pos,&w)||!ma_read_u64(p,n,&pos,&argc)||!ma_read_u64(p,n,&pos,&v)||op>MA_HW_REG||w>64||argc>3)goto fail;c.nodes[i]=(ma_hw_node){(unsigned)op,(unsigned)w,(unsigned)argc,{0},v};for(unsigned j=0;j<3;j++){if(!ma_read_u64(p,n,&pos,&v)||v>SIZE_MAX)goto fail;c.nodes[i].arg[j]=(size_t)v;}}for(size_t i=0;i<c.n;i++)if(!ma_hw_node_valid(&c,i))goto fail;*out=c;return 1;fail:mulib_hwil_context_free(&c);return 0;}
#ifdef __cplusplus
}
#endif
#endif
