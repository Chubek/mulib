/** @file mulib-containerd-cache.h
 * @brief Validated image cache with atomic LRU eviction, expiry and pinned leases.
 * Define MULIB_IMPL_CONTAINERD_CACHE plus IMGFORMAT dependencies. */
#ifndef MULIB_CONTAINERD_CACHE_H
#define MULIB_CONTAINERD_CACHE_H
#include "mulib-imgformat.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_containerd_cache_entry
{
  char key[MULIB_CT_ID_CAP];
  mulib_ct_blob image;
  uint64_t generation, touched, expires;
  size_t pins;
  int active;
} mulib_containerd_cache_entry;
typedef struct mulib_containerd_cache_lease
{ size_t slot; uint64_t generation; } mulib_containerd_cache_lease;
typedef struct mulib_containerd_cache
{
  mulib_containerd_cache_entry *entries;
  size_t capacity, count, bytes, max_bytes;
  uint64_t generation, clock, hits, misses, evictions;
} mulib_containerd_cache;
/** @brief Allocate fixed stable slots with a total image-byte budget. */
int mulib_containerd_cache_init (mulib_containerd_cache *cache, size_t entries, size_t bytes);
/** @brief Free only when no leases remain; returns EBUSY without changing cache. */
int mulib_containerd_cache_free (mulib_containerd_cache *cache);
/** @brief Validate/copy image and atomically replace/evict unpinned LRU entries.
 * TTL zero never expires. now/ttl share caller-defined monotonic units. */
int mulib_containerd_cache_put (mulib_containerd_cache *cache, const char *key,
    const void *image, size_t length, uint64_t now, uint64_t ttl);
/** @brief Pin an unexpired image. Borrowed bytes stay valid until release. */
int mulib_containerd_cache_acquire (mulib_containerd_cache *cache, const char *key,
    uint64_t now, mulib_containerd_cache_lease *lease, const mulib_ct_blob **image);
/** @brief Release one acquisition; each successful acquire requires exactly one release. */
int mulib_containerd_cache_release (mulib_containerd_cache *cache, mulib_containerd_cache_lease lease);
/** @brief Remove an exact unpinned key, including an expired entry. */
int mulib_containerd_cache_remove (mulib_containerd_cache *cache, const char *key);
/** @brief Remove all expired, unpinned entries. */
size_t mulib_containerd_cache_sweep (mulib_containerd_cache *cache, uint64_t now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERD_CACHE) && !defined(MULIB_CONTAINERD_CACHE_IMPLEMENTED)
#define MULIB_CONTAINERD_CACHE_IMPLEMENTED
static void mulib_containerd_cache_drop_ (mulib_containerd_cache *c, size_t i)
{
  mulib_containerd_cache_entry *e = c->entries + i;
  c->bytes -= e->image.length; --c->count;
  mulib_ct_blob_free (&e->image); memset (e, 0, sizeof (*e));
}
static uint64_t mulib_containerd_cache_tick_ (mulib_containerd_cache *c)
{
  size_t i;
  if (c->clock == UINT64_MAX)
    { for (i = 0; i < c->capacity; ++i) c->entries[i].touched = 0; c->clock = 0; }
  return ++c->clock;
}
int mulib_containerd_cache_init (mulib_containerd_cache *c, size_t count, size_t bytes)
{
  mulib_containerd_cache next;
  if (!c || !count || count > SIZE_MAX / sizeof (*next.entries)) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  next.entries = (mulib_containerd_cache_entry *)MULIB_CT_MALLOC (count * sizeof (*next.entries));
  if (!next.entries) return MULIB_CT_ENOMEM;
  memset (next.entries, 0, count * sizeof (*next.entries));
  next.capacity = count; next.max_bytes = bytes; *c = next;
  return 0;
}
int mulib_containerd_cache_free (mulib_containerd_cache *c)
{
  size_t i;
  if (!c) return MULIB_CT_EINVAL;
  for (i = 0; i < c->capacity; ++i) if (c->entries[i].pins) return MULIB_CT_EBUSY;
  for (i = 0; i < c->capacity; ++i) mulib_ct_blob_free (&c->entries[i].image);
  MULIB_CT_FREE (c->entries); memset (c, 0, sizeof (*c));
  return 0;
}
int
mulib_containerd_cache_put (mulib_containerd_cache *c, const char *key,
    const void *data, size_t n, uint64_t now, uint64_t ttl)
{
  mulib_ct_blob copy = { NULL, 0, 0 };
  mulib_imgformat_view view;
  char key_copy[MULIB_CT_ID_CAP];
  unsigned char *drop;
  size_t i, target = SIZE_MAX, remaining, count;
  int rc;
  if (!c || !c->capacity || !mulib_ct_id_valid (key)) return MULIB_CT_EINVAL;
  (void)mulib_ct_text_copy (key_copy, sizeof (key_copy), key); key = key_copy;
  if (ttl > UINT64_MAX - now || c->generation == UINT64_MAX) return MULIB_CT_ELIMIT;
  rc = mulib_imgformat_open (data, n, c->max_bytes, &view);
  if (rc) return rc;
  for (i = 0; i < c->capacity; ++i)
    if (c->entries[i].active && !strcmp (c->entries[i].key, key)) { target = i; break; }
  if (target != SIZE_MAX && c->entries[target].pins) return MULIB_CT_EBUSY;
  rc = mulib_ct_blob_assign (&copy, data, n, c->max_bytes);
  if (rc) return rc;
  drop = (unsigned char *)MULIB_CT_MALLOC (c->capacity);
  if (!drop) { mulib_ct_blob_free (&copy); return MULIB_CT_ENOMEM; }
  memset (drop, 0, c->capacity); remaining = c->bytes; count = c->count;
  if (target != SIZE_MAX)
    { drop[target] = 1; remaining -= c->entries[target].image.length; --count; }
  while (count == c->capacity || n > c->max_bytes - remaining)
    {
      size_t best = SIZE_MAX;
      for (i = 0; i < c->capacity; ++i)
        if (c->entries[i].active && !c->entries[i].pins && !drop[i]
            && (best == SIZE_MAX || c->entries[i].touched < c->entries[best].touched)) best = i;
      if (best == SIZE_MAX)
        { MULIB_CT_FREE (drop); mulib_ct_blob_free (&copy); return MULIB_CT_EBUSY; }
      drop[best] = 1; remaining -= c->entries[best].image.length; --count;
      if (target == SIZE_MAX) target = best;
    }
  if (target == SIZE_MAX)
    for (i = 0; i < c->capacity; ++i) if (!c->entries[i].active) { target = i; break; }
  for (i = 0; i < c->capacity; ++i)
    if (drop[i])
      {
        if (strcmp (c->entries[i].key, key)) c->evictions = mulib_ct_counter_add (c->evictions, 1);
        mulib_containerd_cache_drop_ (c, i);
      }
  MULIB_CT_FREE (drop);
  (void)mulib_ct_text_copy (c->entries[target].key, sizeof (c->entries[target].key), key);
  c->entries[target].image = copy; c->entries[target].active = 1;
  c->entries[target].expires = ttl ? now + ttl : 0;
  c->entries[target].generation = ++c->generation;
  c->entries[target].touched = mulib_containerd_cache_tick_ (c);
  ++c->count; c->bytes += n;
  return 0;
}
int
mulib_containerd_cache_acquire (mulib_containerd_cache *c, const char *key,
    uint64_t now, mulib_containerd_cache_lease *lease, const mulib_ct_blob **image)
{
  size_t i;
  if (!c || !key || !lease || !image) return MULIB_CT_EINVAL;
  for (i = 0; i < c->capacity; ++i)
    {
      mulib_containerd_cache_entry *e = c->entries + i;
      if (!e->active || strcmp (e->key, key) || (e->expires && now >= e->expires)) continue;
      if (e->pins == SIZE_MAX) return MULIB_CT_ELIMIT;
      ++e->pins; e->touched = mulib_containerd_cache_tick_ (c);
      lease->slot = i; lease->generation = e->generation; *image = &e->image;
      c->hits = mulib_ct_counter_add (c->hits, 1); return 0;
    }
  c->misses = mulib_ct_counter_add (c->misses, 1);
  return MULIB_CT_ENOENT;
}
int
mulib_containerd_cache_release (mulib_containerd_cache *c, mulib_containerd_cache_lease lease)
{
  mulib_containerd_cache_entry *e;
  if (!c || lease.slot >= c->capacity) return MULIB_CT_EINVAL;
  e = c->entries + lease.slot;
  if (!e->active || e->generation != lease.generation || !e->pins) return MULIB_CT_ESTALE;
  --e->pins;
  return 0;
}
int mulib_containerd_cache_remove (mulib_containerd_cache *c, const char *key)
{
  size_t i;
  if (!c || !key) return MULIB_CT_EINVAL;
  for (i = 0; i < c->capacity; ++i)
    if (c->entries[i].active && !strcmp (c->entries[i].key, key))
      {
        if (c->entries[i].pins) return MULIB_CT_EBUSY;
        mulib_containerd_cache_drop_ (c, i); return 0;
      }
  return MULIB_CT_ENOENT;
}
size_t mulib_containerd_cache_sweep (mulib_containerd_cache *c, uint64_t now)
{
  size_t i, count = 0;
  if (!c) return 0;
  for (i = 0; i < c->capacity; ++i)
    if (c->entries[i].active && !c->entries[i].pins && c->entries[i].expires
        && now >= c->entries[i].expires) { mulib_containerd_cache_drop_ (c, i); ++count; }
  return count;
}
#endif
