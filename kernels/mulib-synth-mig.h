/* MuLIB: synth-mig. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_MIG in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_MIG_H
#define MULIB_SYNTH_MIG_H
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
typedef struct {unsigned op,literal,invert;size_t a,b,c;} mulib_synth_mig_node;
int mulib_synth_mig_eval(const mulib_synth_mig_node *nodes,size_t n,const unsigned char *inputs,size_t ni,unsigned char *values);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_MIG) && !defined(MULIB_SYNTH_MIG_H_IMPLEMENTED)
#define MULIB_SYNTH_MIG_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_mig_eval(const mulib_synth_mig_node *nodes,size_t n,const unsigned char *inputs,size_t ni,unsigned char *values){if((!nodes&&n)||(!inputs&&ni)||(!values&&n))return 0;size_t in=0;for(size_t i=0;i<n;i++){mulib_synth_mig_node a=nodes[i];if(a.op==0){if(in>=ni)return 0;values[i]=inputs[in++]!=0;}else if(a.op==1)values[i]=a.literal!=0;else{if(a.op!=2||a.a>=i||a.b>=i||a.c>=i)return 0;unsigned x=values[a.a]^(a.invert&1),y=values[a.b]^((a.invert>>1)&1),z=values[a.c]^((a.invert>>2)&1);values[i]=(unsigned char)((x&y)|(x&z)|(y&z));}}return in==ni;}
#ifdef __cplusplus
}
#endif
#endif
