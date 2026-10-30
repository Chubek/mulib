/** @file mulib-vcscomitt.h
 * @brief Compatibility spelling for the canonical vcscommit API.
 * Define MULIB_IMPL_VCSCOMITT and all VCSCOMMIT dependencies separately.
 */
#ifndef MULIB_VCSCOMITT_H
#define MULIB_VCSCOMITT_H
#include "mulib-vcscommit.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_vcscommit_t mulib_vcscomitt_t;
  typedef mulib_vcscommit_identity_t mulib_vcscomitt_identity_t;
  /** @brief Compatibility entry points with identical ownership and failures.
   */
  int mulib_vcscomitt_encode (const mulib_vcscomitt_t *, char **, size_t *);
  int mulib_vcscomitt_store (mulib_vcsrepo_t *, const mulib_vcscomitt_t *,
                             mulib_git_oid_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VCSCOMITT) && !defined(MULIB_VCSCOMITT_IMPLEMENTED)
#define MULIB_VCSCOMITT_IMPLEMENTED
int
mulib_vcscomitt_encode (const mulib_vcscomitt_t *c, char **p, size_t *n)
{
  return mulib_vcscommit_encode (c, p, n);
}
int
mulib_vcscomitt_store (mulib_vcsrepo_t *r, const mulib_vcscomitt_t *c,
                       mulib_git_oid_t *id)
{
  return mulib_vcscommit_store (r, c, id);
}
#endif
