/** @file mulib-ocicontainernetwrork.h
 * @brief Compatibility spelling of the container network-plan module.
 * Define MULIB_IMPL_OCICONTAINERNETWRORK and canonical NETWORK dependencies.
 */
#ifndef MULIB_OCICONTAINERNETWRORK_H
#define MULIB_OCICONTAINERNETWRORK_H
#include "mulib-ocicontainernetwork.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_ocicontainernetwork_t mulib_ocicontainernetwrork_t;
  /** @brief Forward to canonical network-plan validation; 1 success. */
  int mulib_ocicontainernetwrork_init (mulib_ocicontainernetwrork_t *out,
                                       const char *address, unsigned prefix,
                                       const char *gateway);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINERNETWRORK)                                  \
    && !defined(MULIB_OCICONTAINERNETWRORK_IMPLEMENTED)
#define MULIB_OCICONTAINERNETWRORK_IMPLEMENTED
int
mulib_ocicontainernetwrork_init (mulib_ocicontainernetwrork_t *out,
                                 const char *a, unsigned p, const char *g)
{
  return mulib_ocicontainernetwork_init (out, a, p, g);
}
#endif
