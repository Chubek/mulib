/* MuLIB / Amb3r: synth-lutmap. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_LUTMAP in exactly one implementation translation unit.
 * Profile: LUT truth extraction for a selected cut; not full global technology mapping.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_LUTMAP_H
#define MULIB_SYNTH_LUTMAP_H
#include "mulib-amb3r-base.h"
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_lutmap_truth(const ma_aig *a,size_t root,const size_t *leaves,unsigned n,uint64_t *truth);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_LUTMAP) && !defined(MULIB_SYNTH_LUTMAP_H_IMPLEMENTED)
#define MULIB_SYNTH_LUTMAP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_lutmap_truth(const ma_aig *a,size_t root,const size_t *leaves,unsigned n,uint64_t *truth){if(!a||root/2>a->n||n>6||(!leaves&&n)||!truth)return 0;unsigned char *value=(unsigned char*)ma_alloc(a->n+1,1),*known=(unsigned char*)ma_alloc(a->n+1,1);if(!value||!known){free(value);free(known);return 0;}uint64_t t=0;for(unsigned x=0;x<(1u<<n);x++){memset(known,0,a->n+1);known[0]=1;for(unsigned j=0;j<n;j++){if(leaves[j]>=a->n)goto fail;known[leaves[j]+1]=1;value[leaves[j]+1]=(x>>j)&1;}for(size_t i=1;i<=a->n;i++)if(!known[i]){ma_aig_node v=a->v[i-1];if(!v.input&&v.a/2<i&&v.b/2<i&&known[v.a/2]&&known[v.b/2]){known[i]=1;value[i]=(value[v.a/2]^(v.a&1))&(value[v.b/2]^(v.b&1));}}if(!known[root/2])goto fail;t|=(uint64_t)(value[root/2]^(root&1))<<x;}*truth=t;free(value);free(known);return 1;fail:free(value);free(known);return 0;}
#ifdef __cplusplus
}
#endif
#endif
