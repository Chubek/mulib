/* MuLIB: Opaque cursor protocol with explicit error and borrowed-item semantics. C11
 * single-header; define MULIB_IMPL_ITERATOR once. */
#ifndef MULIB_ITERATOR_H
#define MULIB_ITERATOR_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    void *state;
    int (*next) (void *, const void **);
    void (*close) (void *);
  } mulib_iterator_t;
  int mulib_iterator_next (mulib_iterator_t *it, const void **item);
  void mulib_iterator_close (mulib_iterator_t *it);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ITERATOR) && !defined(MULIB_ITERATOR_IMPLEMENTED)
#define MULIB_ITERATOR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_iterator_next (mulib_iterator_t *it, const void **out)
  {
    if (!it || !out || !it->next)
      return -1;
    return it->next (it->state, out);
  }
  void
  mulib_iterator_close (mulib_iterator_t *it)
  {
    if (!it)
      return;
    if (it->close)
      it->close (it->state);
    memset (it, 0, sizeof (*it));
  }
#ifdef __cplusplus
}
#endif
#endif
