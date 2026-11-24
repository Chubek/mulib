/* MuLIB: hwil-use-def. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_USE_DEF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_USE_DEF_H
#define MULIB_HWIL_USE_DEF_H
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

size_t mulib_hwil_use_def_users(const mulib_hwil_hw_context *c,size_t node,size_t *users,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_USE_DEF) && !defined(MULIB_HWIL_USE_DEF_H_IMPLEMENTED)
#define MULIB_HWIL_USE_DEF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_hwil_use_def_users(const mulib_hwil_hw_context *c,size_t node,size_t *users,size_t capacity){if(!c||node>=c->n)return MULIB_STORAGE_NONE;size_t n=0;for(size_t i=0;i<c->n;i++){if(!mulib_hwil_hw_node_valid(c,i))return MULIB_STORAGE_NONE;for(unsigned j=0;j<c->nodes[i].argc;j++)if(c->nodes[i].arg[j]==node){if(users&&n<capacity)users[n]=i;n++;break;}}return n;}
#ifdef __cplusplus
}
#endif
#endif
