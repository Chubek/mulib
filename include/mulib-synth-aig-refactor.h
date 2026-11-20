/* MuLIB: synth-aig-refactor. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_AIG_REFACTOR in exactly one implementation translation unit.
 * Profile: Common-factor expression construction, not general cone refactoring.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_AIG_REFACTOR_H
#define MULIB_SYNTH_AIG_REFACTOR_H
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

int mulib_synth_aig_refactor(mulib_synth_aig_graph *a,size_t common,size_t left,size_t right,size_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_AIG_REFACTOR) && !defined(MULIB_SYNTH_AIG_REFACTOR_H_IMPLEMENTED)
#define MULIB_SYNTH_AIG_REFACTOR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_aig_refactor(mulib_synth_aig_graph *a,size_t common,size_t left,size_t right,size_t *out){if(!a||!out)return 0;size_t old=a->n,merge;if(!mulib_synth_aig_graph_and(a,left^1,right^1,&merge)||!mulib_synth_aig_graph_and(a,common,merge^1,out)){a->n=old;return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
