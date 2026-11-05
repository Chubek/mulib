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
typedef int (*mulib_trie_match_fn)(const void *key, size_t length, uint64_t value,
    size_t distance, void *user);
/** DP-row traversal with subtree pruning, byte Levenshtein radius. Callback
 * borrows key, nonzero continues; return count including stop, SIZE_MAX on
 * invalid/OOM/work limit (callbacks may already have run). Query/key traversal
 * depth <=4096; live DP storage <=64 MiB. max_work=0 means 10000000 cells/edges.
 * Optional work is always initialized; callbacks must not mutate the trie. */
size_t mulib_trie_approximate(const mulib_trie_t *, const void *, size_t, size_t radius,
    size_t max_work, mulib_trie_match_fn, void *, size_t *work);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TRIE) && !defined(MULIB_TRIE_IMPLEMENTED)
#define MULIB_TRIE_IMPLEMENTED
#include <errno.h>
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
size_t mulib_trie_approximate(const mulib_trie_t *t, const void *query, size_t m, size_t radius,
    size_t limit, mulib_trie_match_fn visit, void *user, size_t *work) {
  typedef struct frame { const mulib_trie_node_t *node, *next; size_t *row; } frame;
  frame *stack = NULL;
  const unsigned char *q = (const unsigned char *)query;
  size_t used = 0, spent = 0, calls = 0, i, bytes, minimum;
  size_t *row = NULL;
  const mulib_trie_node_t *node;
  if (work) *work = 0;
  if (!t || (!q && m) || m > 4096 || !visit) { errno = EINVAL; return SIZE_MAX; }
  if (!t->root) return 0;
  if (!limit) limit = 10000000;
  bytes = (m + 1) * sizeof(size_t);
  stack = (frame *)calloc(4097, sizeof *stack); row = (size_t *)malloc(bytes);
  if (!stack || !row) { errno = ENOMEM; goto fail; }
  for (i = 0; i <= m; ++i) row[i] = i;
  stack[0].node = t->root; stack[0].next = t->root->child; stack[0].row = row; row = NULL; used = 1;
  if (t->root->terminal && m <= radius) {
    ++calls; if (!visit(t->root->key, t->root->length, t->root->value, m, user)) goto done;
  }
  while (used) {
    frame *parent = &stack[used - 1];
    if (!parent->next) { free(parent->row); --used; continue; }
    node = parent->next; parent->next = node->next;
    if (m + 1 > limit - spent || used > 4096 || (used + 1) > 67108864u / bytes) { errno = EOVERFLOW; goto fail; }
    spent += m + 1;
    row = (size_t *)malloc(bytes); if (!row) { errno = ENOMEM; goto fail; }
    row[0] = used; minimum = row[0];
    for (i = 1; i <= m; ++i) {
      size_t a = parent->row[i] + 1, b = row[i - 1] + 1;
      size_t c = parent->row[i - 1] + (node->edge != q[i - 1]);
      row[i] = a < b ? a : b; if (c < row[i]) row[i] = c;
      if (row[i] < minimum) minimum = row[i];
    }
    if (node->terminal && row[m] <= radius) {
      ++calls; if (!visit(node->key, node->length, node->value, row[m], user)) { free(row); row = NULL; goto done; }
    }
    if (minimum <= radius && node->child) {
      stack[used].node = node; stack[used].next = node->child; stack[used].row = row; ++used; row = NULL;
    } else { free(row); row = NULL; }
  }
done:
  while (used) free(stack[--used].row);
  free(stack); if (work) *work = spent; return calls;
fail:
  free(row); calls = SIZE_MAX; goto done;
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
