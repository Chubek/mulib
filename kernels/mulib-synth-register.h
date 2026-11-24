/* MuLIB: synth-register. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_REGISTER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_REGISTER_H
#define MULIB_SYNTH_REGISTER_H
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
#include "mulib-graph-core.h"
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_synth_register_duplicates(const mulib_hwil_hw_context *c,size_t *representative);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_REGISTER) && !defined(MULIB_SYNTH_REGISTER_H_IMPLEMENTED)
#define MULIB_SYNTH_REGISTER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_synth_register_duplicates(const mulib_hwil_hw_context *c,size_t *representative){if(!c||(!representative&&c->n))return MULIB_STORAGE_NONE;size_t count=0;for(size_t i=0;i<c->n;i++){representative[i]=i;if(c->nodes[i].op!=MULIB_HWIL_HW_REG)continue;for(size_t j=0;j<i;j++)if(c->nodes[j].op==MULIB_HWIL_HW_REG&&c->nodes[j].width==c->nodes[i].width&&c->nodes[j].arg[0]==c->nodes[i].arg[0]&&c->nodes[j].constant==c->nodes[i].constant){representative[i]=j;count++;break;}}return count;}
#ifdef __cplusplus
}
#endif
#endif
