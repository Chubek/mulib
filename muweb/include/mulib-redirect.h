/** @file mulib-redirect.h
 * @brief HTTP redirect status classification and method rewriting.
 * C11 single-header implementation. Define MULIB_IMPL_REDIRECT in one translation unit.
 */
#ifndef MULIB_REDIRECT_H
#define MULIB_REDIRECT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Return redirect eligibility and decide whether POST becomes GET. */
int mulib_redirect_status(int status);
int mulib_redirect_post_to_get(int status);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_REDIRECT) && !defined(MULIB_IMPL_REDIRECT_IMPLEMENTED)
#define MULIB_IMPL_REDIRECT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_redirect_status(int x){return x==301||x==302||x==303||x==307||x==308;}
int mulib_redirect_post_to_get(int x){return x==301||x==302||x==303;}

#ifdef __cplusplus
}
#endif
#endif
