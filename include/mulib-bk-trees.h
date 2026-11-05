/** @file mulib-bk-trees.h
 * @brief Owned BK tree for byte-string Levenshtein radius queries.
 * Define MULIB_IMPL_BK_TREES and MULIB_IMPL_LEVENSHTEIN.
 */
#ifndef MULIB_BK_TREES_H
#define MULIB_BK_TREES_H
#include "mulib-levenshtein.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_bk_node mulib_bk_node_t;
typedef struct mulib_bk_tree { mulib_bk_node_t *root; size_t size; } mulib_bk_tree_t;
typedef int (*mulib_bk_visit_fn)(const char *word, size_t distance, void *context);
/** Initialize uninitialized storage; zero initialization is also valid. */
void mulib_bk_tree_init(mulib_bk_tree_t *tree);
/** Copy word, ignoring duplicates; return 0 on invalid/OOM without mutation. */
int mulib_bk_tree_insert(mulib_bk_tree_t *tree, const char *word);
/** Metric-pruned radius query; callback borrows word and must not mutate tree.
 * Nonzero continues. Return calls including stop, SIZE_MAX invalid/OOM.
 * A distance-allocation failure may occur after earlier callbacks. Order unspecified. */
size_t mulib_bk_tree_search(const mulib_bk_tree_t *tree, const char *query, size_t radius,
                           mulib_bk_visit_fn visit, void *context);
/** Release iteratively and reset. */
void mulib_bk_tree_free(mulib_bk_tree_t *tree);
/** Budgeted variants: bound total DP cells, optional work initialized on every
 * return. Zero limit selects 10000000. Same mutation/callback contracts. */
int mulib_bk_tree_insert_bounded(mulib_bk_tree_t *, const char *, size_t, size_t *);
size_t mulib_bk_tree_search_bounded(const mulib_bk_tree_t *, const char *, size_t,
    size_t, mulib_bk_visit_fn, void *, size_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BK_TREES) && !defined(MULIB_BK_TREES_IMPLEMENTED)
#define MULIB_BK_TREES_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_bk_node { struct mulib_bk_node *parent, *child, *next; char *word; size_t edge; };
void mulib_bk_tree_init(mulib_bk_tree_t *t) { if (t) { t->root = NULL; t->size = 0; } }
static int mulib_bk_cells_(const char *a, const char *b, size_t *work, size_t limit) {
  size_t n = strlen(a), m = strlen(b);
  if (n == SIZE_MAX || m == SIZE_MAX || n + 1 > (limit - *work) / (m + 1)) return 0;
  *work += (n + 1) * (m + 1); return 1;
}
int mulib_bk_tree_insert_bounded(mulib_bk_tree_t *t, const char *word, size_t limit, size_t *out_work)
{
  mulib_bk_node_t **slot, *parent = NULL, *n;
  size_t edge = 0, length, work = 0;
  if (out_work) *out_work = 0;
  if (!limit) limit = 10000000;
  if (!t || !word || t->size == SIZE_MAX) return 0;
  slot = &t->root;
  while (*slot) {
    parent = *slot;
    if (!mulib_bk_cells_(parent->word, word, &work, limit)) { if (out_work) *out_work = work; errno = EOVERFLOW; return 0; }
    if (out_work) *out_work = work;
    edge = mulib_levenshtein_distance(parent->word, word);
    if (edge == SIZE_MAX) { errno = ENOMEM; return 0; }
    if (!edge) return 1;
    slot = &parent->child;
    while (*slot && (*slot)->edge != edge) slot = &(*slot)->next;
  }
  length = strlen(word);
  if (length == SIZE_MAX) return 0;
  n = (mulib_bk_node_t *)calloc(1, sizeof(*n));
  if (!n) { errno = ENOMEM; return 0; }
  n->word = (char *)malloc(length + 1);
  if (!n->word) { free(n); errno = ENOMEM; return 0; }
  memcpy(n->word, word, length + 1); n->parent = parent; n->edge = edge; *slot = n; ++t->size; return 1;
}
int mulib_bk_tree_insert(mulib_bk_tree_t *t, const char *word)
{ return mulib_bk_tree_insert_bounded(t, word, SIZE_MAX, NULL); }
size_t mulib_bk_tree_search_bounded(const mulib_bk_tree_t *t, const char *q, size_t radius,
                           size_t limit, mulib_bk_visit_fn fn, void *ctx, size_t *out_work)
{
  const mulib_bk_node_t **stack;
  size_t used = 0, calls = 0, work = 0;
  if (out_work) *out_work = 0;
  if (!limit) limit = 10000000;
  if (!t || !q || !fn || t->size > SIZE_MAX / sizeof(*stack)) return SIZE_MAX;
  if (!t->root) return 0;
  stack = (const mulib_bk_node_t **)malloc(t->size * sizeof(*stack));
  if (!stack) { errno = ENOMEM; return SIZE_MAX; }
  stack[used++] = t->root;
  while (used) {
    const mulib_bk_node_t *n = stack[--used];
    size_t d;
    if (!mulib_bk_cells_(n->word, q, &work, limit)) { free(stack); if (out_work) *out_work = work; errno = EOVERFLOW; return SIZE_MAX; }
    if (out_work) *out_work = work;
    d = mulib_levenshtein_distance(n->word, q);
    if (d == SIZE_MAX) { free(stack); errno = ENOMEM; return SIZE_MAX; }
    if (d <= radius) { ++calls; if (!fn(n->word, d, ctx)) break; }
    for (n = n->child; n; n = n->next)
      if ((n->edge > d ? n->edge - d : d - n->edge) <= radius) stack[used++] = n;
  }
  free(stack); return calls;
}
size_t mulib_bk_tree_search(const mulib_bk_tree_t *t, const char *q, size_t r, mulib_bk_visit_fn fn, void *u)
{ return mulib_bk_tree_search_bounded(t, q, r, SIZE_MAX, fn, u, NULL); }
void mulib_bk_tree_free(mulib_bk_tree_t *t)
{
  mulib_bk_node_t *n;
  if (!t) return;
  n = t->root;
  while (n) {
    if (n->child) { mulib_bk_node_t *c = n->child; n->child = c->next; c->next = NULL; n = c; }
    else { mulib_bk_node_t *p = n->parent; free(n->word); free(n); n = p; }
  }
  mulib_bk_tree_init(t);
}
#ifdef __cplusplus
}
#endif
#endif
