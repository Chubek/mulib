/** @file mulib-containerd.h
 * @brief Embeddable container daemon core composing pools, caches, exports and logs.
 * Define MULIB_IMPL_CONTAINERD and all transitive switches explicitly. */
#ifndef MULIB_CONTAINERD_H
#define MULIB_CONTAINERD_H
#include "mulib-containterpool.h"
#include "mulib-containerd-cache.h"
#include "mulib-containerd-mount.h"
#include "mulib-container-log.h"
#include "mulib-conainerd-ipc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_containerd_options
{
  size_t containers, backends, cache_entries, cache_bytes;
  size_t log_records, log_bytes, log_message_bytes, mount_bindings, transfer_bytes;
  mulib_image2container_limits image_limits;
} mulib_containerd_options;
/** @brief Owns subsystems. Keep at a stable address; externally serialize all access. */
typedef struct mulib_containerd
{
  mulib_containerd_options options;
  mulib_containterpool pool;
  mulib_containerboost backends;
  mulib_containerd_cache cache;
  mulib_containerd_mount mounts;
  mulib_container_log log;
  uint64_t commands, failures, log_failures;
  int initialized, busy;
} mulib_containerd;
/** @brief Conservative nonzero defaults for every bounded subsystem. */
mulib_containerd_options mulib_containerd_default_options (void);
/** @brief Initialize all subsystems atomically; register backends before create. */
int mulib_containerd_init (mulib_containerd *daemon, const mulib_containerd_options *options);
/** @brief Shutdown inactive/unborrowed entries and release subsystems. Running
 * containers return EBUSY. Backend close failure leaves remaining resources live. */
int mulib_containerd_free (mulib_containerd *daemon, uint64_t now);
/** @brief Materialize, prepare and export one image. No process starts implicitly. */
int mulib_containerd_create (mulib_containerd *daemon, const void *image, size_t length,
    const char *id, const char *backend, uint32_t required_capabilities, uint64_t now);
/** @brief Create from a temporarily pinned cache entry. */
int mulib_containerd_create_cached (mulib_containerd *daemon, const char *cache_key,
    const char *id, const char *backend, uint32_t required_capabilities, uint64_t now);
/** @brief Execute one validated command with optional generation compare-and-swap.
 * Caller supplies trusted monotonic command.now. Returns CT error; success
 * writes status (DELETE returns the final DELETED snapshot). */
int mulib_containerd_dispatch (mulib_containerd *daemon, const mulib_conainerd_command *command,
                               mulib_conainerd_status *status);
/** @brief Advance fair cooperative execution and sweep expired cache entries. */
int mulib_containerd_tick (mulib_containerd *daemon, size_t visits, size_t budget,
                           uint64_t now, mulib_containterpool_report *report);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERD) && !defined(MULIB_CONTAINERD_IMPLEMENTED)
#define MULIB_CONTAINERD_IMPLEMENTED
mulib_containerd_options mulib_containerd_default_options (void)
{
  mulib_containerd_options o;
  memset (&o, 0, sizeof (o)); o.containers = 64; o.backends = 16;
  o.cache_entries = 32; o.cache_bytes = 128U * 1024U * 1024U;
  o.log_records = 1024; o.log_bytes = 1024U * 1024U; o.log_message_bytes = 4096;
  o.mount_bindings = 16; o.transfer_bytes = 1024U * 1024U;
  o.image_limits = mulib_image2container_default_limits ();
  return o;
}
int mulib_containerd_init (mulib_containerd *d, const mulib_containerd_options *options)
{
  mulib_containerd next;
  int rc;
  if (!d) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  next.options = options ? *options : mulib_containerd_default_options ();
  if (!next.options.mount_bindings) return MULIB_CT_EINVAL;
  rc = mulib_containterpool_init (&next.pool, next.options.containers);
  if (!rc) rc = mulib_containerboost_init (&next.backends, next.options.backends);
  if (!rc) rc = mulib_containerd_cache_init (&next.cache, next.options.cache_entries, next.options.cache_bytes);
  if (!rc) rc = mulib_containerd_mount_init (&next.mounts, next.options.containers);
  if (!rc) rc = mulib_container_log_init (&next.log, next.options.log_records,
      next.options.log_bytes, next.options.log_message_bytes, 1);
  if (rc)
    {
      mulib_container_log_free (&next.log); (void)mulib_containerd_mount_free (&next.mounts);
      (void)mulib_containerd_cache_free (&next.cache); mulib_containerboost_free (&next.backends);
      (void)mulib_containterpool_free (&next.pool, 0); return rc;
    }
  next.initialized = 1; *d = next;
  return 0;
}
static void mulib_containerd_log_ (mulib_containerd *d, const char *id, const char *text,
                                  uint64_t now, int error)
{
  if (mulib_container_log_append (&d->log, id,
      error ? MULIB_CONTAINER_LOG_ERROR : MULIB_CONTAINER_LOG_INFO,
      0, now, text, strlen (text), NULL)) d->log_failures = mulib_ct_counter_add (d->log_failures, 1);
}
static void mulib_containerd_status_ (const mulib_containterpool_entry *e, mulib_conainerd_status *s)
{
  const mulib_container *c = &e->image.container;
  s->state = c->state; s->exit_code = c->exit_code; s->generation = c->generation;
  s->starts = c->starts; s->started_at = c->started_at; s->exited_at = c->exited_at;
  s->polls = e->runtime.polls;
}
int mulib_containerd_free (mulib_containerd *d, uint64_t now)
{
  size_t i;
  int rc;
  if (!d) return MULIB_CT_EINVAL;
  if (!d->initialized) return 0;
  if (d->busy) return MULIB_CT_EBUSY;
  for (i = 0; i < d->mounts.capacity; ++i)
    if (d->mounts.entries[i].references || d->mounts.entries[i].session.busy) return MULIB_CT_EBUSY;
  for (i = 0; i < d->cache.capacity; ++i) if (d->cache.entries[i].pins) return MULIB_CT_EBUSY;
  for (i = 0; i < d->pool.capacity; ++i)
    if (d->pool.slots[i] && (d->pool.slots[i]->references
        || d->pool.slots[i]->image.container.state == MULIB_CONTAINER_RUNNING
        || d->pool.slots[i]->image.container.state == MULIB_CONTAINER_PAUSED)) return MULIB_CT_EBUSY;
  d->busy = 1;
  for (i = 0; i < d->pool.capacity; ++i)
    if (d->pool.slots[i])
      {
        mulib_containterpool_entry *e = d->pool.slots[i];
        mulib_containterpool_handle h;
        h.slot = i; h.generation = e->generation;
        d->pool.busy = 1; rc = mulib_container_runtime_close (&e->runtime, now); d->pool.busy = 0;
        if (rc) { d->busy = 0; return rc; }
        rc = mulib_containerd_mount_remove (&d->mounts, e->image.container.spec.id);
        if (rc && rc != MULIB_CT_ENOENT) { d->busy = 0; return rc; }
        rc = mulib_containterpool_remove (&d->pool, h, now);
        if (rc) { d->busy = 0; return rc; }
      }
  rc = mulib_containterpool_free (&d->pool, now); d->busy = 0;
  if (rc) return rc;
  rc = mulib_containerd_mount_free (&d->mounts);
  if (rc) return rc;
  rc = mulib_containerd_cache_free (&d->cache);
  if (rc) return rc;
  mulib_containerboost_free (&d->backends); mulib_container_log_free (&d->log);
  memset (d, 0, sizeof (*d));
  return 0;
}
int
mulib_containerd_create (mulib_containerd *d, const void *image, size_t n,
    const char *id, const char *backend, uint32_t caps, uint64_t now)
{
  mulib_containterpool_handle h;
  mulib_containterpool_entry *e;
  int rc;
  if (!d || !d->initialized) return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  d->busy = 1;
  rc = mulib_containterpool_add (&d->pool, image, n, id, &d->options.image_limits,
                                 &d->backends, backend, caps, &h);
  if (!rc)
    {
      e = d->pool.slots[h.slot];
      rc = mulib_containerd_mount_add (&d->mounts, e->image.container.spec.id,
          &e->image.rootfs, e->image.container.spec.read_only,
          d->options.mount_bindings, d->options.transfer_bytes);
      if (rc) (void)mulib_containterpool_remove (&d->pool, h, now);
      else mulib_containerd_log_ (d, e->image.container.spec.id, "created", now, 0);
    }
  d->busy = 0;
  return rc;
}
int
mulib_containerd_create_cached (mulib_containerd *d, const char *key, const char *id,
    const char *backend, uint32_t caps, uint64_t now)
{
  mulib_containerd_cache_lease lease;
  const mulib_ct_blob *image;
  int rc;
  if (!d || !d->initialized) return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  rc = mulib_containerd_cache_acquire (&d->cache, key, now, &lease, &image);
  if (rc) return rc;
  rc = mulib_containerd_create (d, image->data, image->length, id, backend, caps, now);
  (void)mulib_containerd_cache_release (&d->cache, lease);
  return rc;
}
int
mulib_containerd_dispatch (mulib_containerd *d, const mulib_conainerd_command *c,
                           mulib_conainerd_status *out)
{
  mulib_containterpool_handle h;
  mulib_containterpool_entry *e;
  mulib_conainerd_status status;
  size_t i;
  int rc;
  if (!d || !d->initialized || !c || !out || !mulib_ct_id_valid (c->id)
      || c->opcode < 1 || c->opcode > MULIB_CONAINERD_STEP) return MULIB_CT_EINVAL;
  if ((c->opcode == MULIB_CONAINERD_STOP && (!c->value || c->value > 64))
      || (c->opcode == MULIB_CONAINERD_STEP && (!c->value || c->value > SIZE_MAX))
      || (c->opcode != MULIB_CONAINERD_STOP && c->opcode != MULIB_CONAINERD_STEP && c->value))
    return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  d->commands = mulib_ct_counter_add (d->commands, 1);
  rc = mulib_containterpool_find (&d->pool, c->id, &h);
  if (rc) { d->failures = mulib_ct_counter_add (d->failures, 1); return rc; }
  e = d->pool.slots[h.slot];
  if (c->expected_generation && c->expected_generation != e->image.container.generation)
    { d->failures = mulib_ct_counter_add (d->failures, 1); return MULIB_CT_ESTALE; }
  d->busy = 1;
  switch (c->opcode)
    {
    case MULIB_CONAINERD_START: rc = mulib_container_runtime_start (&e->runtime, c->now); break;
    case MULIB_CONAINERD_STOP: rc = mulib_container_runtime_signal (&e->runtime, (unsigned)c->value); break;
    case MULIB_CONAINERD_PAUSE: rc = mulib_container_runtime_pause (&e->runtime, 1, c->now); break;
    case MULIB_CONAINERD_RESUME: rc = mulib_container_runtime_pause (&e->runtime, 0, c->now); break;
    case MULIB_CONAINERD_STEP:
      rc = mulib_container_runtime_step (&e->runtime, (size_t)c->value, c->now);
      if (rc == MULIB_CT_DONE || rc == MULIB_CT_AGAIN) rc = 0;
      break;
    case MULIB_CONAINERD_STATUS: rc = 0; break;
    case MULIB_CONAINERD_DELETE:
      rc = e->references ? MULIB_CT_EBUSY : 0;
      for (i = 0; i < d->mounts.capacity && !rc; ++i)
        if (d->mounts.entries[i].active && !strcmp (d->mounts.entries[i].id, c->id)
            && (d->mounts.entries[i].references || d->mounts.entries[i].session.busy)) rc = MULIB_CT_EBUSY;
      if (!rc)
        {
          mulib_containerd_status_ (e, &status);
          d->pool.busy = 1;
          rc = mulib_container_runtime_close (&e->runtime, c->now);
          d->pool.busy = 0;
        }
      if (!rc)
        {
          status.state = e->image.container.state;
          status.generation = e->image.container.generation;
          rc = mulib_containerd_mount_remove (&d->mounts, c->id);
          if (!rc || rc == MULIB_CT_ENOENT)
            rc = mulib_containterpool_remove (&d->pool, h, c->now);
        }
      break;
    default: rc = MULIB_CT_ENOTSUP; break;
    }
  if (!rc && c->opcode != MULIB_CONAINERD_DELETE) mulib_containerd_status_ (e, &status);
  if (rc) d->failures = mulib_ct_counter_add (d->failures, 1);
  if (c->opcode != MULIB_CONAINERD_STATUS)
    mulib_containerd_log_ (d, c->id, rc ? mulib_ct_strerror (rc) : mulib_container_state_name (status.state), c->now, rc);
  d->busy = 0; if (!rc) *out = status;
  return rc;
}
int
mulib_containerd_tick (mulib_containerd *d, size_t visits, size_t budget,
                       uint64_t now, mulib_containterpool_report *report)
{
  int rc;
  if (!d || !d->initialized) return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  d->busy = 1; rc = mulib_containterpool_tick (&d->pool, visits, budget, now, report);
  (void)mulib_containerd_cache_sweep (&d->cache, now); d->busy = 0;
  return rc;
}
#endif
