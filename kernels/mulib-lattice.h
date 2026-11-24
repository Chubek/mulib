/* MuLIB: Finite 64-attribute implication closure lattice with join/meet of closed sets.
 * C11 single-header; define MULIB_IMPL_LATTICE once. */
#ifndef MULIB_LATTICE_H
#define MULIB_LATTICE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t premise, conclusion;
  } mulib_lattice_rule_t;
  typedef struct
  {
    size_t attributes, count, capacity;
    mulib_lattice_rule_t *rules;
  } mulib_lattice_t;
  int mulib_lattice_add_rule (mulib_lattice_t *l, uint64_t premise,
                              uint64_t conclusion);
  uint64_t mulib_lattice_closure (const mulib_lattice_t *l, uint64_t attributes);
  uint64_t mulib_lattice_join (const mulib_lattice_t *l, uint64_t a, uint64_t b);
  uint64_t mulib_lattice_meet (const mulib_lattice_t *l, uint64_t a, uint64_t b);
  void mulib_lattice_free (mulib_lattice_t *l);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LATTICE) && !defined(MULIB_LATTICE_IMPLEMENTED)
#define MULIB_LATTICE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_lattice_mask_ (size_t n)
  {
    return n == 64 ? UINT64_MAX : ((UINT64_C (1) << n) - 1);
  }
  int
  mulib_lattice_add_rule (mulib_lattice_t *l, uint64_t a, uint64_t b)
  {
    if (!l || !l->attributes || l->attributes > 64
        || ((a | b) & ~mulib_lattice_mask_ (l->attributes)))
      return 0;
    if (l->count == l->capacity)
      {
        size_t nc = l->capacity ? l->capacity * 2 : 8;
        if (nc < l->capacity || nc > SIZE_MAX / sizeof (*l->rules))
          return 0;
        void *p = realloc (l->rules, nc * sizeof (*l->rules));
        if (!p)
          return 0;
        l->rules = (mulib_lattice_rule_t *)p;
        l->capacity = nc;
      }
    l->rules[l->count++] = (mulib_lattice_rule_t){ a, b };
    return 1;
  }
  uint64_t
  mulib_lattice_closure (const mulib_lattice_t *l, uint64_t x)
  {
    if (!l || !l->attributes || l->attributes > 64)
      return 0;
    uint64_t mask = mulib_lattice_mask_ (l->attributes);
    x &= mask;
    uint64_t previous;
    do
      {
        previous = x;
        for (size_t i = 0; i < l->count; i++)
          if ((x & l->rules[i].premise) == l->rules[i].premise)
            x |= l->rules[i].conclusion;
      }
    while (previous != x);
    return x & mask;
  }
  uint64_t
  mulib_lattice_join (const mulib_lattice_t *l, uint64_t a, uint64_t b)
  {
    return mulib_lattice_closure (l, a | b);
  }
  uint64_t
  mulib_lattice_meet (const mulib_lattice_t *l, uint64_t a, uint64_t b)
  {
    return mulib_lattice_closure (l, mulib_lattice_closure (l, a)
                                         & mulib_lattice_closure (l, b));
  }
  void
  mulib_lattice_free (mulib_lattice_t *l)
  {
    if (!l)
      return;
    free (l->rules);
    memset (l, 0, sizeof (*l));
  }
#ifdef __cplusplus
}
#endif
#endif
