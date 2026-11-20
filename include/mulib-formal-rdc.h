/* MuLIB: formal-rdc. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_RDC in exactly one implementation translation unit.
 * Profile: Check declared reset-domain crossings and synchronization flags, not structural RDC analysis.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_RDC_H
#define MULIB_FORMAL_RDC_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t source_domain,destination_domain;unsigned synchronized;} mulib_formal_reset_crossing;
size_t mulib_formal_rdc(const mulib_formal_reset_crossing *edges,size_t n,size_t *violations,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_RDC) && !defined(MULIB_FORMAL_RDC_H_IMPLEMENTED)
#define MULIB_FORMAL_RDC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_formal_rdc(const mulib_formal_reset_crossing *edges,size_t n,size_t *violations,size_t capacity){if(!edges&&n)return MULIB_STORAGE_NONE;size_t count=0;for(size_t i=0;i<n;i++)if(edges[i].source_domain!=edges[i].destination_domain&&!edges[i].synchronized){if(violations&&count<capacity)violations[count]=i;count++;}return count;}
#ifdef __cplusplus
}
#endif
#endif
