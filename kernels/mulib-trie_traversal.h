/** @file mulib-trie_traversal.h
 * @brief Allocation-free lexicographic prefix traversal and longest-prefix lookup.
 * Define MULIB_IMPL_TRIE_TRAVERSAL; nodes/keys are borrowed from mulib-trie.
 */
#ifndef MULIB_TRIE_TRAVERSAL_H
#define MULIB_TRIE_TRAVERSAL_H
#include "mulib-trie.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_trie_visit_fn)(const void *key, size_t length, uint64_t value, void *context);
/** Visit prefix matches in unsigned-byte order. Nonzero continues; no mutation
 * from callbacks. Return calls including stop, SIZE_MAX for invalid arguments. */
size_t mulib_trie_traverse(const mulib_trie_t *trie, const void *prefix, size_t length,
                          mulib_trie_visit_fn visit, void *context);
/** Find longest stored prefix of key. Return 1 if found, optional outputs.
 * Includes a stored empty key. Outputs are untouched when not found. */
int mulib_trie_longest_prefix(const mulib_trie_t *trie, const void *key, size_t length,
                             size_t *matched_length, uint64_t *value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TRIE_TRAVERSAL) && !defined(MULIB_TRIE_TRAVERSAL_IMPLEMENTED)
#define MULIB_TRIE_TRAVERSAL_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static const mulib_trie_node_t *mulib_trie_traversal_child_(const mulib_trie_node_t *n, unsigned char c)
{
  n = n->child;
  while (n && n->edge < c) n = n->next;
  return n && n->edge == c ? n : NULL;
}
size_t mulib_trie_traverse(const mulib_trie_t *t, const void *prefix, size_t len,
                          mulib_trie_visit_fn fn, void *ctx)
{
  const unsigned char *p = (const unsigned char *)prefix;
  const mulib_trie_node_t *n, *root;
  size_t calls = 0;
  if (!t || (!prefix && len) || !fn) return SIZE_MAX;
  n = t->root;
  for (size_t i = 0; n && i < len; ++i) n = mulib_trie_traversal_child_(n, p[i]);
  root = n;
  while (n) {
    if (n->terminal) { ++calls; if (!fn(n->key, n->length, n->value, ctx)) break; }
    if (n->child) { n = n->child; continue; }
    while (n != root && !n->next) n = n->parent;
    n = n == root ? NULL : n->next;
  }
  return calls;
}
int mulib_trie_longest_prefix(const mulib_trie_t *t, const void *key, size_t len,
                             size_t *length, uint64_t *out)
{
  const unsigned char *k = (const unsigned char *)key;
  const mulib_trie_node_t *n, *best = NULL;
  if (!t || (!key && len)) return 0;
  n = t->root;
  for (size_t i = 0; n;) {
    if (n->terminal) best = n;
    if (i == len) break;
    n = mulib_trie_traversal_child_(n, k[i++]);
  }
  if (!best) return 0;
  if (length) *length = best->length;
  if (out) *out = best->value;
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
