/* MuLIB: spice-elaborate. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_ELABORATE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_ELABORATE_H
#define MULIB_SPICE_ELABORATE_H
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

int mulib_spice_elaborate_nodes(const char *const *names,size_t n,size_t *ids,size_t *node_count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_ELABORATE) && !defined(MULIB_SPICE_ELABORATE_H_IMPLEMENTED)
#define MULIB_SPICE_ELABORATE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_elaborate_nodes(const char *const *names,size_t n,size_t *ids,size_t *node_count){if((!names&&n)||(!ids&&n)||!node_count)return 0;size_t count=0;for(size_t i=0;i<n;i++){if(!names[i]||!*names[i])return 0;if(!strcmp(names[i],"0")||!strcmp(names[i],"gnd")){ids[i]=MULIB_STORAGE_NONE;continue;}size_t j=0;for(;j<i;j++)if(!strcmp(names[j],names[i]))break;ids[i]=j<i?ids[j]:count++;}*node_count=count;return 1;}
#ifdef __cplusplus
}
#endif
#endif
