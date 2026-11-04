/** @file mulib-n-grams.h
 * @brief Borrowed sliding byte n-gram enumeration.
 * Define MULIB_IMPL_N_GRAMS. No tokenization, padding or allocation.
 */
#ifndef MULIB_N_GRAMS_H
#define MULIB_N_GRAMS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_ngram_visit_fn)(const unsigned char *gram, size_t length, size_t offset, void *context);
/** Visit length-width+1 windows, including overlaps. Nonzero callback continues.
 * Return callback count (including stopping call), SIZE_MAX for invalid/zero width. */
size_t mulib_ngrams(const void *bytes, size_t length, size_t width, mulib_ngram_visit_fn visit, void *context);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_N_GRAMS) && !defined(MULIB_N_GRAMS_IMPLEMENTED)
#define MULIB_N_GRAMS_IMPLEMENTED
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_ngrams(const void *p, size_t n, size_t width, mulib_ngram_visit_fn fn, void *ctx)
{
  const unsigned char *s = (const unsigned char *)p;
  size_t count = 0;
  if ((!p && n) || !width || !fn) return SIZE_MAX;
  if (width > n) return 0;
  for (size_t i = 0; i <= n - width; ++i) { ++count; if (!fn(s + i, width, i, ctx)) break; }
  return count;
}
#ifdef __cplusplus
}
#endif
#endif
