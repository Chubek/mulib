/* MuLIB / Amb3r: synth-aig-refactor. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_AIG_REFACTOR in exactly one implementation translation unit.
 * Profile: Common-factor expression construction, not general cone refactoring.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_AIG_REFACTOR_H
#define MULIB_SYNTH_AIG_REFACTOR_H
#include "mulib-amb3r-base.h"
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_aig_refactor(ma_aig *a,size_t common,size_t left,size_t right,size_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_AIG_REFACTOR) && !defined(MULIB_SYNTH_AIG_REFACTOR_H_IMPLEMENTED)
#define MULIB_SYNTH_AIG_REFACTOR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_aig_refactor(ma_aig *a,size_t common,size_t left,size_t right,size_t *out){if(!a||!out)return 0;size_t old=a->n,merge;if(!ma_aig_and(a,left^1,right^1,&merge)||!ma_aig_and(a,common,merge^1,out)){a->n=old;return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
