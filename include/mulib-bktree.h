/** @file mulib-bktree.h
 * @brief Canonical BK-tree interface reusing MuLIB's metric-pruned byte BK tree.
 * Define MULIB_IMPL_BKTREE, MULIB_IMPL_BK_TREES and MULIB_IMPL_LEVENSHTEIN.
 * Radius distances are byte Levenshtein, not weighted/nonmetric distances.
 */
#ifndef MULIB_BKTREE_H
#define MULIB_BKTREE_H
#include "mulib-bk-trees.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_bk_tree_t mulib_bktree_t;
typedef mulib_bk_visit_fn mulib_bktree_visit_fn;
/** Zero initialization is valid; insert copies words. */
void mulib_bktree_init(mulib_bktree_t *);
int mulib_bktree_insert(mulib_bktree_t *, const char *);
/** Return callback count or SIZE_MAX on invalid/OOM. May deliver partial results
 * on allocation failure; callback words are borrowed until tree destruction. */
size_t mulib_bktree_search(const mulib_bktree_t *, const char *, size_t,
    mulib_bktree_visit_fn, void *);
void mulib_bktree_free(mulib_bktree_t *);
int mulib_bktree_insert_bounded(mulib_bktree_t *, const char *, size_t max_cells, size_t *work);
size_t mulib_bktree_search_bounded(const mulib_bktree_t *, const char *, size_t radius,
    size_t max_cells, mulib_bktree_visit_fn, void *, size_t *work);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BKTREE) && !defined(MULIB_BKTREE_IMPLEMENTED)
#define MULIB_BKTREE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_bktree_init(mulib_bktree_t *t) { mulib_bk_tree_init(t); }
int mulib_bktree_insert(mulib_bktree_t *t, const char *s) { return mulib_bk_tree_insert(t, s); }
size_t mulib_bktree_search(const mulib_bktree_t *t, const char *s, size_t r,
    mulib_bktree_visit_fn f, void *u) { return mulib_bk_tree_search(t, s, r, f, u); }
void mulib_bktree_free(mulib_bktree_t *t) { mulib_bk_tree_free(t); }
int mulib_bktree_insert_bounded(mulib_bktree_t *t, const char *s, size_t l, size_t *w)
{ return mulib_bk_tree_insert_bounded(t, s, l, w); }
size_t mulib_bktree_search_bounded(const mulib_bktree_t *t, const char *s, size_t r, size_t l,
    mulib_bktree_visit_fn f, void *u, size_t *w) { return mulib_bk_tree_search_bounded(t, s, r, l, f, u, w); }
#ifdef __cplusplus
}
#endif
#endif
