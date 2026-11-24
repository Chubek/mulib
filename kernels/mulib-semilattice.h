/* MuLIB: Grow-only 64-bit-set join/meet semilattice and partial-order tests. C11
 * single-header; define MULIB_IMPL_SEMILATTICE once. */
#ifndef MULIB_SEMILATTICE_H
#define MULIB_SEMILATTICE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_semilattice_join (uint64_t a, uint64_t b);
  uint64_t mulib_semilattice_meet (uint64_t a, uint64_t b);
  int mulib_semilattice_leq (uint64_t a, uint64_t b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEMILATTICE) && !defined(MULIB_SEMILATTICE_IMPLEMENTED)
#define MULIB_SEMILATTICE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t
  mulib_semilattice_join (uint64_t a, uint64_t b)
  {
    return a | b;
  }
  uint64_t
  mulib_semilattice_meet (uint64_t a, uint64_t b)
  {
    return a & b;
  }
  int
  mulib_semilattice_leq (uint64_t a, uint64_t b)
  {
    return (a & b) == a;
  }
#ifdef __cplusplus
}
#endif
#endif
