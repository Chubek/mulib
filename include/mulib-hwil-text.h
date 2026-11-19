/* MuLIB / Amb3r: hwil-text. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_TEXT in exactly one implementation translation unit.
 * Profile: Private HWIL1 numeric interchange, not a complete hardware assembly language.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_TEXT_H
#define MULIB_HWIL_TEXT_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_text_write(FILE *f,const ma_hw_context *c);
int mulib_hwil_text_read(FILE *f,size_t limit,ma_hw_context *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_TEXT) && !defined(MULIB_HWIL_TEXT_H_IMPLEMENTED)
#define MULIB_HWIL_TEXT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_text_write(FILE *f,const ma_hw_context *c){if(!f||!c||fprintf(f,"HWIL1 %zu\n",c->n)<0)return 0;for(size_t i=0;i<c->n;i++){ma_hw_node n=c->nodes[i];if(!ma_hw_node_valid(c,i)||fprintf(f,"%u %u %u %llu %zu %zu %zu\n",n.op,n.width,n.argc,(unsigned long long)n.constant,n.arg[0],n.arg[1],n.arg[2])<0)return 0;}return 1;}
int mulib_hwil_text_read(FILE *f,size_t limit,ma_hw_context *out){if(!f||!out||out->nodes)return 0;char magic[8];size_t n;if(fscanf(f,"%7s %zu",magic,&n)!=2||strcmp(magic,"HWIL1")||n>limit)return 0;ma_hw_context c={0};if(!ma_reserve((void**)&c.nodes,&c.cap,n,sizeof *c.nodes))return 0;c.n=n;for(size_t i=0;i<n;i++){ma_hw_node *p=c.nodes+i;unsigned long long v;if(fscanf(f,"%u %u %u %llu %zu %zu %zu",&p->op,&p->width,&p->argc,&v,&p->arg[0],&p->arg[1],&p->arg[2])!=7)goto fail;p->constant=(uint64_t)v;}for(size_t i=0;i<n;i++)if(!ma_hw_node_valid(&c,i))goto fail;*out=c;return 1;fail:mulib_hwil_context_free(&c);return 0;}
#ifdef __cplusplus
}
#endif
#endif
