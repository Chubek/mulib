/**
 * @file mulib-cowb+tree.h
 * @brief Reference-counted B+ tree snapshots with whole-tree copy on first
 * write. Define MULIB_IMPL_COWBPTREE and MULIB_IMPL_BPTREE once.
 * Single-threaded refs.
 */
#ifndef MULIB_COWBPTREE_H
#define MULIB_COWBPTREE_H
#include "mulib-b+tree.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_cowbptree_state mulib_cowbptree_state_t;
  typedef struct mulib_cowbptree
  {
    mulib_cowbptree_state_t *state;
  } mulib_cowbptree_t;
  /** @brief Share immutable state into an uninitialized destination; 1
   * succeeds. */
  int mulib_cowbptree_snapshot (const mulib_cowbptree_t *src,
                                mulib_cowbptree_t *out);
  /** @brief Insert/replace, detaching shared storage atomically; zero init
   * required. */
  int mulib_cowbptree_set (mulib_cowbptree_t *t, int64_t key, uint64_t value);
  /** @brief Lookup; 1 found, 0 absent/invalid, optional value unchanged on
   * miss. */
  int mulib_cowbptree_get (const mulib_cowbptree_t *t, int64_t key,
                           uint64_t *value);
  /** @brief Remove; 1 removed, 0 absent or allocation failure. */
  int mulib_cowbptree_remove (mulib_cowbptree_t *t, int64_t key);
  /** @brief Release this reference and reset. */
  void mulib_cowbptree_free (mulib_cowbptree_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COWBPTREE) && !defined(MULIB_COWBPTREE_IMPLEMENTED)
#define MULIB_COWBPTREE_IMPLEMENTED
#include <stdlib.h>
struct mulib_cowbptree_state
{
  size_t refs;
  mulib_bptree_t tree;
};
typedef struct mulib_cowbptree_copy_context_
{
  mulib_bptree_t *tree;
  int ok;
} mulib_cowbptree_copy_context_;
static int
mulib_cowbptree_copy_record_ (int64_t k, uint64_t v, void *ctx)
{
  mulib_cowbptree_copy_context_ *c = (mulib_cowbptree_copy_context_ *)ctx;
  c->ok = mulib_bptree_set (c->tree, k, v);
  return c->ok;
}
static mulib_cowbptree_state_t *
mulib_cowbptree_copy_ (const mulib_cowbptree_t *t)
{
  mulib_cowbptree_state_t *s
      = (mulib_cowbptree_state_t *)calloc (1, sizeof (*s));
  mulib_cowbptree_copy_context_ c;
  if (!s)
    return NULL;
  s->refs = 1;
  c.tree = &s->tree;
  c.ok = 1;
  if (t->state)
    mulib_bptree_range (&t->state->tree, INT64_MIN, INT64_MAX,
                        mulib_cowbptree_copy_record_, &c);
  if (!c.ok)
    {
      mulib_bptree_free (&s->tree);
      free (s);
      return NULL;
    }
  return s;
}
void
mulib_cowbptree_free (mulib_cowbptree_t *t)
{
  if (!t)
    return;
  if (t->state && !--t->state->refs)
    {
      mulib_bptree_free (&t->state->tree);
      free (t->state);
    }
  t->state = NULL;
}
int
mulib_cowbptree_snapshot (const mulib_cowbptree_t *s, mulib_cowbptree_t *out)
{
  if (!s || !out || s == out || (s->state && s->state->refs == SIZE_MAX))
    return 0;
  if (s->state)
    ++s->state->refs;
  *out = *s;
  return 1;
}
int
mulib_cowbptree_set (mulib_cowbptree_t *t, int64_t k, uint64_t v)
{
  mulib_cowbptree_state_t *s;
  if (!t)
    return 0;
  if (t->state && t->state->refs == 1)
    return mulib_bptree_set (&t->state->tree, k, v);
  s = mulib_cowbptree_copy_ (t);
  if (!s)
    return 0;
  if (!mulib_bptree_set (&s->tree, k, v))
    {
      mulib_bptree_free (&s->tree);
      free (s);
      return 0;
    }
  mulib_cowbptree_free (t);
  t->state = s;
  return 1;
}
int
mulib_cowbptree_get (const mulib_cowbptree_t *t, int64_t k, uint64_t *v)
{
  return t && t->state && mulib_bptree_get (&t->state->tree, k, v);
}
int
mulib_cowbptree_remove (mulib_cowbptree_t *t, int64_t k)
{
  mulib_cowbptree_state_t *s;
  if (!mulib_cowbptree_get (t, k, NULL))
    return 0;
  if (t->state->refs == 1)
    return mulib_bptree_remove (&t->state->tree, k, NULL);
  s = mulib_cowbptree_copy_ (t);
  if (!s)
    return 0;
  mulib_bptree_remove (&s->tree, k, NULL);
  mulib_cowbptree_free (t);
  t->state = s;
  return 1;
}
#endif
