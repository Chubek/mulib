/* MuLIB / Amb3r: synth-multiplexer. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_MULTIPLEXER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_MULTIPLEXER_H
#define MULIB_SYNTH_MULTIPLEXER_H
#include "mulib-amb3r-base.h"
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_multiplexer(ma_aig *g,size_t select,size_t a,size_t b,size_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_MULTIPLEXER) && !defined(MULIB_SYNTH_MULTIPLEXER_H_IMPLEMENTED)
#define MULIB_SYNTH_MULTIPLEXER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_multiplexer(ma_aig *g,size_t select,size_t a,size_t b,size_t *out){if(!g||!out)return 0;size_t old=g->n,x,y,z;if(!ma_aig_and(g,select,a,&x)||!ma_aig_and(g,select^1,b,&y)||!ma_aig_and(g,x^1,y^1,&z)){g->n=old;return 0;}*out=z^1;return 1;}
#ifdef __cplusplus
}
#endif
#endif
