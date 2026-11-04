/** @file mulib-automata-levenshtein.h
 * @brief Streaming Levenshtein recognizer using capped dynamic-programming states.
 * Define MULIB_IMPL_AUTOMATA_LEVENSHTEIN. Owns a copied binary pattern.
 */
#ifndef MULIB_AUTOMATA_LEVENSHTEIN_H
#define MULIB_AUTOMATA_LEVENSHTEIN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_levenshtein_automaton mulib_levenshtein_automaton_t;
/** Create a recognizer accepting distance <= limit; SIZE_MAX limit is invalid. */
mulib_levenshtein_automaton_t *mulib_levenshtein_automaton_create(const void *pattern, size_t n, size_t limit);
/** Reset to the empty candidate without allocating. NULL is harmless. */
void mulib_levenshtein_automaton_reset(mulib_levenshtein_automaton_t *a);
/** Consume candidate bytes. Return 1, or 0 for invalid arguments. */
int mulib_levenshtein_automaton_feed(mulib_levenshtein_automaton_t *a, const void *bytes, size_t n);
/** Return whether the current candidate is accepted; NULL gives zero. */
int mulib_levenshtein_automaton_accepts(const mulib_levenshtein_automaton_t *a);
/** Release all storage. */
void mulib_levenshtein_automaton_destroy(mulib_levenshtein_automaton_t *a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AUTOMATA_LEVENSHTEIN) && !defined(MULIB_AUTOMATA_LEVENSHTEIN_IMPLEMENTED)
#define MULIB_AUTOMATA_LEVENSHTEIN_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_levenshtein_automaton { unsigned char *pattern; size_t *row, n, limit; };
void mulib_levenshtein_automaton_reset(mulib_levenshtein_automaton_t *a)
{
  if (a) for (size_t j = 0; j <= a->n; ++j) a->row[j] = j <= a->limit ? j : a->limit + 1;
}
void mulib_levenshtein_automaton_destroy(mulib_levenshtein_automaton_t *a)
{ if (a) { free(a->pattern); free(a->row); free(a); } }
mulib_levenshtein_automaton_t *mulib_levenshtein_automaton_create(const void *p, size_t n, size_t limit)
{
  mulib_levenshtein_automaton_t *a;
  if ((!p && n) || limit == SIZE_MAX || n >= SIZE_MAX / sizeof(size_t)) return NULL;
  a = (mulib_levenshtein_automaton_t *)calloc(1, sizeof(*a));
  if (!a) return NULL;
  a->pattern = (unsigned char *)malloc(n ? n : 1);
  a->row = (size_t *)malloc((n + 1) * sizeof(size_t));
  if (!a->pattern || !a->row) { mulib_levenshtein_automaton_destroy(a); return NULL; }
  if (n) memcpy(a->pattern, p, n);
  a->n = n; a->limit = limit; mulib_levenshtein_automaton_reset(a); return a;
}
static size_t mulib_levenshtein_automaton_add_(size_t x, size_t cap)
{ return x < cap ? x + 1 : cap; }
int mulib_levenshtein_automaton_feed(mulib_levenshtein_automaton_t *a, const void *p, size_t n)
{
  const unsigned char *s = (const unsigned char *)p;
  if (!a || (!p && n)) return 0;
  for (size_t i = 0; i < n; ++i) {
    size_t diagonal = a->row[0], cap = a->limit + 1;
    a->row[0] = mulib_levenshtein_automaton_add_(a->row[0], cap);
    for (size_t j = 1; j <= a->n; ++j) {
      size_t old = a->row[j], v = diagonal;
      if (a->pattern[j - 1] != s[i]) v = mulib_levenshtein_automaton_add_(v, cap);
      size_t up = mulib_levenshtein_automaton_add_(old, cap);
      size_t left = mulib_levenshtein_automaton_add_(a->row[j - 1], cap);
      if (up < v) v = up;
      if (left < v) v = left;
      a->row[j] = v; diagonal = old;
    }
  }
  return 1;
}
int mulib_levenshtein_automaton_accepts(const mulib_levenshtein_automaton_t *a)
{ return a && a->row[a->n] <= a->limit; }
#ifdef __cplusplus
}
#endif
#endif
