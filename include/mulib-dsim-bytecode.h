/* MuLIB / Amb3r: dsim-bytecode. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_BYTECODE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_BYTECODE_H
#define MULIB_DSIM_BYTECODE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned op,literal;size_t dst,a,b;} ma_sim_instruction;
int mulib_dsim_bytecode_run(const ma_sim_instruction *code,size_t n,unsigned *r,size_t nr);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_BYTECODE) && !defined(MULIB_DSIM_BYTECODE_H_IMPLEMENTED)
#define MULIB_DSIM_BYTECODE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_bytecode_run(const ma_sim_instruction *code,size_t n,unsigned *r,size_t nr){if((!code&&n)||(!r&&nr))return 0;for(size_t i=0;i<n;i++){ma_sim_instruction c=code[i];if(c.dst>=nr||c.op>4)return 0;unsigned x;if(c.op==0)x=c.literal;else{if(c.a>=nr||r[c.a]>3)return 0;unsigned a=r[c.a],b=0;if(c.op>=2){if(c.b>=nr||r[c.b]>3)return 0;b=r[c.b];}if(c.op==1)x=a<2?1-a:2;else if(c.op==2)x=a==0||b==0?0:a==1&&b==1?1:2;else if(c.op==3)x=a==1||b==1?1:a==0&&b==0?0:2;else x=a<2&&b<2?a^b:2;}if(x>3)return 0;r[c.dst]=x;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
