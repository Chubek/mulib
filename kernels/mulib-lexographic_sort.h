/** @file mulib-lexographic_sort.h
 * @brief Locale-independent unsigned-byte lexicographic string sorting.
 * Define MULIB_IMPL_LEXOGRAPHIC_SORT. Historical filename retained.
 */
#ifndef MULIB_LEXOGRAPHIC_SORT_H
#define MULIB_LEXOGRAPHIC_SORT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Compare C strings, optionally folding ASCII A-Z; NULL sorts before non-NULL. */
int mulib_lexographic_compare(const char *a, const char *b, int fold_ascii);
/** Stable in-place sort of borrowed pointers. Return 0 on NULL entries/invalid
 * array, before mutation. Quadratic time, no allocations. */
int mulib_lexographic_sort(const char **strings, size_t n, int fold_ascii);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LEXOGRAPHIC_SORT) && !defined(MULIB_LEXOGRAPHIC_SORT_IMPLEMENTED)
#define MULIB_LEXOGRAPHIC_SORT_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_lexographic_compare(const char *a, const char *b, int fold)
{
  if (!a || !b) return a ? 1 : b ? -1 : 0;
  for (;;) {
    unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
    if (fold) { if (x >= 'A' && x <= 'Z') x += 'a' - 'A'; if (y >= 'A' && y <= 'Z') y += 'a' - 'A'; }
    if (x != y) return x < y ? -1 : 1;
    if (!x) return 0;
  }
}
int mulib_lexographic_sort(const char **s, size_t n, int fold)
{
  if (!s && n) return 0;
  for (size_t i = 0; i < n; ++i) if (!s[i]) return 0;
  for (size_t i = 1; i < n; ++i) {
    const char *value = s[i]; size_t j = i;
    while (j && mulib_lexographic_compare(s[j - 1], value, fold) > 0) { s[j] = s[j - 1]; --j; }
    s[j] = value;
  }
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
