/** @file mulib-redirect.h
 * @brief HTTP redirect status classification and method rewriting.
 * C11 single-header implementation. Define MULIB_IMPL_REDIRECT in one
 * translation unit.
 */
#ifndef MULIB_REDIRECT_H
#define MULIB_REDIRECT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief 1 for 301/302/303/307/308, otherwise 0. No Location processing. */
  int mulib_redirect_status (int status);
  /** @brief Browser-style POST-to-GET policy for 301/302/303 only. This is not
   * a general method rewrite rule: e.g. HEAD must not become GET for 303. */
  int mulib_redirect_post_to_get (int status);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_REDIRECT) && !defined(MULIB_REDIRECT_IMPLEMENTED)
#define MULIB_REDIRECT_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  int
  mulib_redirect_status (int x)
  {
    return x == 301 || x == 302 || x == 303 || x == 307 || x == 308;
  }
  int
  mulib_redirect_post_to_get (int x)
  {
    return x == 301 || x == 302 || x == 303;
  }

#ifdef __cplusplus
}
#endif
#endif
