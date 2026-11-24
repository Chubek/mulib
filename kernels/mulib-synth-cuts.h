/* MuLIB: synth-cuts. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_CUTS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_CUTS_H
#define MULIB_SYNTH_CUTS_H
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
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned n;size_t leaf[8];} mulib_synth_cut;
size_t mulib_synth_cuts(const mulib_synth_aig_graph *a,size_t root,unsigned k,mulib_synth_cut *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_CUTS) && !defined(MULIB_SYNTH_CUTS_H_IMPLEMENTED)
#define MULIB_SYNTH_CUTS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_synth_cuts(const mulib_synth_aig_graph *a,size_t root,unsigned k,mulib_synth_cut *out,size_t cap){if(!a||root>=a->n||!out||!cap||!k||k>8)return 0;out[0]=(mulib_synth_cut){1,{root}};size_t count=1;for(size_t i=0;i<count&&count<cap;i++)for(unsigned j=0;j<out[i].n&&count<cap;j++){mulib_synth_cut c=out[i];size_t v=c.leaf[j];if(v>=a->n||a->v[v].input)continue;c.leaf[j]=c.leaf[--c.n];size_t arg[2]={a->v[v].a/2,a->v[v].b/2};int ok=1;for(unsigned z=0;z<2;z++)if(arg[z]){size_t id=arg[z]-1;if(id>=v){ok=0;break;}unsigned h=0;while(h<c.n&&c.leaf[h]!=id)h++;if(h==c.n){if(c.n==k){ok=0;break;}c.leaf[c.n++]=id;}}if(!ok)continue;for(unsigned z=1;z<c.n;z++){size_t x=c.leaf[z];unsigned h=z;while(h&&c.leaf[h-1]>x){c.leaf[h]=c.leaf[h-1];h--;}c.leaf[h]=x;}size_t h=0;for(;h<count;h++)if(out[h].n==c.n&&!memcmp(out[h].leaf,c.leaf,c.n*sizeof(size_t)))break;if(h==count)out[count++]=c;}return count;}
#ifdef __cplusplus
}
#endif
#endif
