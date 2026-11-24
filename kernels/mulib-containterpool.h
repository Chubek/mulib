/** @file mulib-containterpool.h
 * @brief Stable owned container slots, generation handles and fair cooperative polling.
 * Requested containterpool spelling is retained. Define MULIB_IMPL_CONTAINTERPOOL. */
#ifndef MULIB_CONTAINTERPOOL_H
#define MULIB_CONTAINTERPOOL_H
#include "mulib-image2container.h"
#include "mulib-containerboost.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_containterpool_handle
{ size_t slot; uint64_t generation; } mulib_containterpool_handle;
typedef struct mulib_containterpool_entry
{
  mulib_image2container image;
  mulib_container_runtime runtime;
  uint64_t generation;
  size_t references;
} mulib_containterpool_entry;
typedef struct mulib_containterpool
{
  mulib_containterpool_entry **slots;
  size_t capacity, count, cursor;
  uint64_t generation;
  int busy;
} mulib_containterpool;
typedef struct mulib_containterpool_report
{ size_t visited, polled, exited, failed; int first_error; } mulib_containterpool_report;
/** @brief Initialize a fixed slot table; entry addresses remain stable. */
int mulib_containterpool_init (mulib_containterpool *pool, size_t capacity);
/** @brief Close/free inactive entries. Running, paused or borrowed entries
 * reject preflight. Backend close failures retain the remaining entries. */
int mulib_containterpool_free (mulib_containterpool *pool, uint64_t now);
/** @brief Atomically materialize and prepare one image using an explicit registry. */
int mulib_containterpool_add (mulib_containterpool *pool, const void *image, size_t length,
    const char *id, const mulib_image2container_limits *limits,
    const mulib_containerboost *backends, const char *preferred, uint32_t capabilities,
    mulib_containterpool_handle *handle);
/** @brief Resolve an ID into a generation-tagged handle. */
int mulib_containterpool_find (const mulib_containterpool *pool, const char *id,
                               mulib_containterpool_handle *handle);
/** @brief Pin a stable entry. One release is required for each acquire. */
int mulib_containterpool_acquire (mulib_containterpool *pool, mulib_containterpool_handle handle,
                                  mulib_containterpool_entry **entry);
/** @brief Drop one reference, rejecting stale handles. */
int mulib_containterpool_release (mulib_containterpool *pool, mulib_containterpool_handle handle);
/** @brief Destroy an inactive, unborrowed entry. Backend errors retain ownership. */
int mulib_containterpool_remove (mulib_containterpool *pool, mulib_containterpool_handle handle,
                                 uint64_t now);
/** @brief Visit at most max_visits slots, polling each running/paused entry once.
 * Cursor advances across failures; report records errors without starving peers. */
int mulib_containterpool_tick (mulib_containterpool *pool, size_t max_visits,
    size_t per_container_budget, uint64_t now, mulib_containterpool_report *report);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINTERPOOL) && !defined(MULIB_CONTAINTERPOOL_IMPLEMENTED)
#define MULIB_CONTAINTERPOOL_IMPLEMENTED
static mulib_containterpool_entry *
mulib_containterpool_entry_ (const mulib_containterpool *p, mulib_containterpool_handle h)
{
  if (!p || h.slot >= p->capacity || !p->slots[h.slot]
      || p->slots[h.slot]->generation != h.generation) return NULL;
  return p->slots[h.slot];
}
int mulib_containterpool_init (mulib_containterpool *p, size_t capacity)
{
  mulib_containterpool next;
  if (!p || !capacity || capacity > SIZE_MAX / sizeof (*next.slots)) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  next.slots = (mulib_containterpool_entry **)MULIB_CT_MALLOC (capacity * sizeof (*next.slots));
  if (!next.slots) return MULIB_CT_ENOMEM;
  memset (next.slots, 0, capacity * sizeof (*next.slots)); next.capacity = capacity; *p = next;
  return 0;
}
int mulib_containterpool_find (const mulib_containterpool *p, const char *id, mulib_containterpool_handle *h)
{
  size_t i;
  if (!p || !id || !h) return MULIB_CT_EINVAL;
  for (i = 0; i < p->capacity; ++i)
    if (p->slots[i] && !strcmp (p->slots[i]->image.container.spec.id, id))
      { h->slot = i; h->generation = p->slots[i]->generation; return 0; }
  return MULIB_CT_ENOENT;
}
int
mulib_containterpool_add (mulib_containterpool *p, const void *image, size_t n,
    const char *id, const mulib_image2container_limits *limits,
    const mulib_containerboost *backends, const char *preferred, uint32_t caps,
    mulib_containterpool_handle *handle)
{
  mulib_containterpool_entry *entry;
  mulib_containterpool_handle duplicate;
  size_t i;
  int rc;
  if (!p || !p->capacity || !handle || !backends) return MULIB_CT_EINVAL;
  if (p->busy) return MULIB_CT_EBUSY;
  if (p->count == p->capacity || p->generation == UINT64_MAX) return MULIB_CT_ELIMIT;
  entry = (mulib_containterpool_entry *)MULIB_CT_MALLOC (sizeof (*entry));
  if (!entry) return MULIB_CT_ENOMEM;
  memset (entry, 0, sizeof (*entry));
  rc = mulib_image2container_open (image, n, id, limits, &entry->image, NULL);
  if (rc) { MULIB_CT_FREE (entry); return rc; }
  if (!mulib_containterpool_find (p, entry->image.container.spec.id, &duplicate))
    { mulib_image2container_free (&entry->image); MULIB_CT_FREE (entry); return MULIB_CT_EEXIST; }
  p->busy = 1;
  rc = mulib_containerboost_prepare (backends, entry->image.architecture, caps, preferred,
      &entry->image.container, &entry->image.rootfs, &entry->runtime);
  p->busy = 0;
  if (rc) { mulib_image2container_free (&entry->image); MULIB_CT_FREE (entry); return rc; }
  for (i = 0; i < p->capacity && p->slots[i]; ++i) { }
  entry->generation = ++p->generation; p->slots[i] = entry; ++p->count;
  handle->slot = i; handle->generation = entry->generation;
  return 0;
}
int
mulib_containterpool_acquire (mulib_containterpool *p, mulib_containterpool_handle h,
                             mulib_containterpool_entry **out)
{
  mulib_containterpool_entry *e = mulib_containterpool_entry_ (p, h);
  if (!out) return MULIB_CT_EINVAL;
  if (p && p->busy) return MULIB_CT_EBUSY;
  if (!e) return MULIB_CT_ESTALE;
  if (e->references == SIZE_MAX) return MULIB_CT_ELIMIT;
  ++e->references; *out = e;
  return 0;
}
int mulib_containterpool_release (mulib_containterpool *p, mulib_containterpool_handle h)
{
  mulib_containterpool_entry *e = mulib_containterpool_entry_ (p, h);
  if (p && p->busy) return MULIB_CT_EBUSY;
  if (!e || !e->references) return MULIB_CT_ESTALE;
  --e->references;
  return 0;
}
int mulib_containterpool_remove (mulib_containterpool *p, mulib_containterpool_handle h, uint64_t now)
{
  mulib_containterpool_entry *e = mulib_containterpool_entry_ (p, h);
  int rc;
  if (!e) return MULIB_CT_ESTALE;
  if (p->busy || e->references) return MULIB_CT_EBUSY;
  p->busy = 1; rc = mulib_container_runtime_close (&e->runtime, now); p->busy = 0;
  if (rc) return rc;
  mulib_image2container_free (&e->image); MULIB_CT_FREE (e); p->slots[h.slot] = NULL; --p->count;
  return 0;
}
int mulib_containterpool_free (mulib_containterpool *p, uint64_t now)
{
  size_t i;
  if (!p) return MULIB_CT_EINVAL;
  if (p->busy) return MULIB_CT_EBUSY;
  for (i = 0; i < p->capacity; ++i)
    if (p->slots[i] && (p->slots[i]->references
        || p->slots[i]->image.container.state == MULIB_CONTAINER_RUNNING
        || p->slots[i]->image.container.state == MULIB_CONTAINER_PAUSED)) return MULIB_CT_EBUSY;
  for (i = 0; i < p->capacity; ++i)
    if (p->slots[i])
      {
        mulib_containterpool_handle h;
        int rc;
        h.slot = i; h.generation = p->slots[i]->generation;
        rc = mulib_containterpool_remove (p, h, now);
        if (rc) return rc;
      }
  MULIB_CT_FREE (p->slots); memset (p, 0, sizeof (*p));
  return 0;
}
int
mulib_containterpool_tick (mulib_containterpool *p, size_t visits, size_t budget,
                          uint64_t now, mulib_containterpool_report *out)
{
  mulib_containterpool_report r;
  size_t i;
  memset (&r, 0, sizeof (r)); if (out) *out = r;
  if (!p || !p->capacity || !budget) return MULIB_CT_EINVAL;
  if (p->busy) return MULIB_CT_EBUSY;
  if (visits > p->capacity) visits = p->capacity;
  p->busy = 1;
  for (i = 0; i < visits; ++i)
    {
      mulib_containterpool_entry *e = p->slots[p->cursor];
      int rc;
      p->cursor = (p->cursor + 1) % p->capacity; ++r.visited;
      if (!e || (e->image.container.state != MULIB_CONTAINER_RUNNING
          && e->image.container.state != MULIB_CONTAINER_PAUSED)) continue;
      ++r.polled; rc = mulib_container_runtime_step (&e->runtime, budget, now);
      if (rc == MULIB_CT_DONE) ++r.exited;
      if (rc < 0) { ++r.failed; if (!r.first_error) r.first_error = rc; }
    }
  p->busy = 0; if (out) *out = r;
  return r.first_error;
}
#endif
