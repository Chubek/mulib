/** @file mulib-trie.h
 * @brief Owned binary-key byte trie with sorted sibling edges and uint64 values.
 * Define MULIB_IMPL_TRIE. Zero-initialize objects; nodes are read-only to clients.
 */
#ifndef MULIB_TRIE_H
#define MULIB_TRIE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_trie_node {
  struct mulib_trie_node *parent, *child, *next;
  unsigned char edge, *key;
  size_t length;
  uint64_t value;
  int terminal;
} mulib_trie_node_t;
typedef struct mulib_trie { mulib_trie_node_t *root; size_t size; } mulib_trie_t;
/** Initialize an uninitialized object (do not use on a live trie). */
void mulib_trie_init(mulib_trie_t *trie);
/** Copy key and insert/replace. NULL key requires zero length.
 * Return 1 on success; failure preserves all mappings. */
int mulib_trie_set(mulib_trie_t *trie, const void *key, size_t length, uint64_t value);
/** Return whether found, optionally copying the value. */
int mulib_trie_get(const mulib_trie_t *trie, const void *key, size_t length, uint64_t *value);
/** Remove and prune without allocation; return whether found. */
int mulib_trie_remove(mulib_trie_t *trie, const void *key, size_t length);
/** Free iteratively and reset. NULL is harmless. */
void mulib_trie_free(mulib_trie_t *trie);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TRIE) && !defined(MULIB_TRIE_IMPLEMENTED)
#define MULIB_TRIE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_trie_init(mulib_trie_t *t) { if (t) { t->root = NULL; t->size = 0; } }
static mulib_trie_node_t *mulib_trie_find_(const mulib_trie_t *t, const unsigned char *k, size_t len)
{
  mulib_trie_node_t *n = t->root;
  for (size_t i = 0; n && i < len; ++i) {
    n = n->child;
    while (n && n->edge < k[i]) n = n->next;
    if (n && n->edge != k[i]) n = NULL;
  }
  return n;
}
int mulib_trie_get(const mulib_trie_t *t, const void *k, size_t len, uint64_t *out)
{
  mulib_trie_node_t *n;
  if (!t || (!k && len)) return 0;
  n = mulib_trie_find_(t, (const unsigned char *)k, len);
  if (!n || !n->terminal) return 0;
  if (out) *out = n->value;
  return 1;
}
void mulib_trie_free(mulib_trie_t *t)
{
  mulib_trie_node_t *n;
  if (!t) return;
  n = t->root;
  while (n) {
    if (n->child) {
      mulib_trie_node_t *c = n->child; n->child = c->next; c->next = NULL; n = c;
    } else {
      mulib_trie_node_t *p = n->parent; free(n->key); free(n); n = p;
    }
  }
  mulib_trie_init(t);
}
int mulib_trie_set(mulib_trie_t *t, const void *key, size_t len, uint64_t value)
{
  const unsigned char *k = (const unsigned char *)key;
  mulib_trie_node_t **slot, *n, *parent = NULL, *first = NULL, *last = NULL;
  unsigned char *copy;
  size_t i = 0;
  if (!t || (!k && len)) return 0;
  n = mulib_trie_find_(t, k, len);
  if (n && n->terminal) { n->value = value; return 1; }
  if (t->size == SIZE_MAX) return 0;
  copy = (unsigned char *)malloc(len ? len : 1);
  if (!copy) return 0;
  if (len) memcpy(copy, k, len);
  slot = &t->root;
  if (t->root) {
    n = t->root;
    while (i < len) {
      parent = n; slot = &n->child;
      while (*slot && (*slot)->edge < k[i]) slot = &(*slot)->next;
      if (!*slot || (*slot)->edge != k[i]) break;
      n = *slot; ++i;
    }
    if (i == len) {
      n->key = copy; n->length = len; n->terminal = 1; n->value = value; ++t->size; return 1;
    }
  }
  /* Build the missing suffix detached, including a root if necessary. */
  for (;;) {
    n = (mulib_trie_node_t *)calloc(1, sizeof(*n));
    if (!n) {
      while (first) { n = first->child; free(first); first = n; }
      free(copy); return 0;
    }
    if (last) { last->child = n; n->parent = last; n->edge = k[i++]; }
    else { first = n; n->parent = parent; if (parent) n->edge = k[i++]; }
    last = n;
    if (i == len) break;
  }
  last->key = copy; last->length = len; last->value = value; last->terminal = 1;
  first->next = *slot; *slot = first; ++t->size; return 1;
}
int mulib_trie_remove(mulib_trie_t *t, const void *key, size_t len)
{
  mulib_trie_node_t *n;
  if (!t || (!key && len)) return 0;
  n = mulib_trie_find_(t, (const unsigned char *)key, len);
  if (!n || !n->terminal) return 0;
  free(n->key); n->key = NULL; n->terminal = 0; --t->size;
  while (n && !n->terminal && !n->child) {
    mulib_trie_node_t *p = n->parent, **slot = p ? &p->child : &t->root;
    while (*slot != n) slot = &(*slot)->next;
    *slot = n->next; free(n); n = p;
  }
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
