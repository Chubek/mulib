/** @file mulib-browsercontext.h
 * @brief Owned browser context identifiers and private-mode flag.
 * C11 single-header implementation. Define MULIB_IMPL_BROWSERCONTEXT in one translation unit.
 */
#ifndef MULIB_BROWSERCONTEXT_H
#define MULIB_BROWSERCONTEXT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_browsercontext {uint64_t id;int private_mode;} mulib_browsercontext_t;
int mulib_browsercontext_init(mulib_browsercontext_t *c,uint64_t id,int private_mode);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BROWSERCONTEXT) && !defined(MULIB_IMPL_BROWSERCONTEXT_IMPLEMENTED)
#define MULIB_IMPL_BROWSERCONTEXT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_browsercontext_init(mulib_browsercontext_t *c,uint64_t id,int p){if(!c||!id)return 0;c->id=id;c->private_mode=!!p;return 1;}

#ifdef __cplusplus
}
#endif
#endif
