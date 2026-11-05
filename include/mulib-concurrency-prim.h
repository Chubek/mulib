/**
 * @file mulib-concurrency-prim.h
 * @brief Fiber-aware mutexes, semaphores, wait groups, events and reader/writer locks.
 * Define MULIB_IMPL_CONCURRENCY_PRIM and dependency switches independently.
 */
#ifndef MULIB_CONCURRENCY_PRIM_H
#define MULIB_CONCURRENCY_PRIM_H
#include "mulib-fiber.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_semaphore mulib_semaphore_t;
typedef struct mulib_mutex mulib_mutex_t;
typedef struct mulib_waitgroup mulib_waitgroup_t;
typedef struct mulib_sync_event mulib_sync_event_t;
typedef struct mulib_rwlock mulib_rwlock_t;
enum {
  MULIB_CONCURRENCY_OK = 0, MULIB_CONCURRENCY_INVALID = -1,
  MULIB_CONCURRENCY_NOMEM = -2, MULIB_CONCURRENCY_SYSTEM = -3,
  MULIB_CONCURRENCY_BUSY = -4, MULIB_CONCURRENCY_TIMEOUT = -5,
  MULIB_CONCURRENCY_OVERFLOW = -6
};
/** Create a counting semaphore; 0 <= initial <= maximum, maximum > 0. */
int mulib_semaphore_create (mulib_semaphore_t **out, size_t initial, size_t maximum);
/** Acquire one permit; relative timeout 0 tries once, UINT64_MAX waits forever. */
int mulib_semaphore_wait (mulib_semaphore_t *, uint64_t timeout_ms);
/** Add permits atomically; overflow leaves the count unchanged. */
int mulib_semaphore_post (mulib_semaphore_t *, size_t count);
/** Destroy with no concurrent callers; BUSY if waiters remain. NULL safe. */
int mulib_semaphore_destroy (mulib_semaphore_t *);
/** Allocate a nonrecursive, fiber-aware mutex. */
int mulib_mutex_create (mulib_mutex_t **out);
/** Acquire with relative timeout; 0 is trylock, UINT64_MAX waits forever. */
int mulib_mutex_lock (mulib_mutex_t *, uint64_t timeout_ms);
/** Unlock; only the acquiring execution context may call this. */
int mulib_mutex_unlock (mulib_mutex_t *);
/** Destroy with no concurrent callers; BUSY if held or waited on. */
int mulib_mutex_destroy (mulib_mutex_t *);
/** Create an initially zero wait group. */
int mulib_waitgroup_create (mulib_waitgroup_t **out);
/** Add work before starting any wait on a new generation. Checks overflow. */
int mulib_waitgroup_add (mulib_waitgroup_t *, size_t count);
/** Complete one unit; INVALID if the counter is already zero. */
int mulib_waitgroup_done (mulib_waitgroup_t *);
/** Wait for zero. Reuse only after all waits from the old generation return. */
int mulib_waitgroup_wait (mulib_waitgroup_t *, uint64_t timeout_ms);
/** Destroy a zero group with no users; otherwise BUSY. */
int mulib_waitgroup_destroy (mulib_waitgroup_t *);
/** Allocate a manual-reset event with the specified initial signaled state. */
int mulib_sync_event_create (mulib_sync_event_t **out, int signaled);
/** Set the event and wake waiters. */
int mulib_sync_event_set (mulib_sync_event_t *);
/** Clear the event. Waiters observe the state when they reacquire its lock. */
int mulib_sync_event_reset (mulib_sync_event_t *);
/** Wait until signaled, without consuming the signal. */
int mulib_sync_event_wait (mulib_sync_event_t *, uint64_t timeout_ms);
/** Destroy with no concurrent callers; BUSY if waiters remain. */
int mulib_sync_event_destroy (mulib_sync_event_t *);
/** Allocate a writer-preferring reader/writer lock; upgrades are unsupported. */
int mulib_rwlock_create (mulib_rwlock_t **out);
/** Acquire shared ownership; 0 tries once, UINT64_MAX waits forever. */
int mulib_rwlock_read_lock (mulib_rwlock_t *, uint64_t timeout_ms);
/** Acquire exclusive ownership. Recursive acquisition is unsupported. */
int mulib_rwlock_write_lock (mulib_rwlock_t *, uint64_t timeout_ms);
/** Release one shared acquisition from its owning execution context. */
int mulib_rwlock_read_unlock (mulib_rwlock_t *);
/** Release exclusive ownership from its owning execution context. */
int mulib_rwlock_write_unlock (mulib_rwlock_t *);
/** Destroy an unheld lock with no users; otherwise BUSY. */
int mulib_rwlock_destroy (mulib_rwlock_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CONCURRENCY_PRIM) && !defined(MULIB_CONCURRENCY_PRIM_IMPLEMENTED)
#define MULIB_CONCURRENCY_PRIM_IMPLEMENTED
#include <stdlib.h>
typedef struct mulib_prim_waiter_ {
  struct mulib_prim_waiter_ *next;
  mulib_fiber_t *fiber;
} mulib_prim_waiter_;
typedef struct mulib_prim_core_ {
  mulib_xmutex_t mutex;
  mulib_xcond_t changed;
  mulib_prim_waiter_ *waiters;
} mulib_prim_core_;
struct mulib_semaphore { mulib_prim_core_ core; size_t count, maximum; };
struct mulib_mutex { mulib_semaphore_t *semaphore; };
struct mulib_waitgroup { mulib_prim_core_ core; size_t count; };
struct mulib_sync_event { mulib_prim_core_ core; int signaled; };
struct mulib_rwlock { mulib_prim_core_ core; size_t readers, writers_waiting; int writer; };
static int mulib_prim_init_ (mulib_prim_core_ *c)
{
  c->waiters = NULL;
  if (mulib_xmutex_init (&c->mutex)) return MULIB_CONCURRENCY_SYSTEM;
  if (mulib_xcond_init (&c->changed)) {
    mulib_xmutex_destroy (&c->mutex); return MULIB_CONCURRENCY_SYSTEM;
  }
  return 0;
}
static void mulib_prim_free_ (mulib_prim_core_ *c)
{ mulib_xcond_destroy (&c->changed); mulib_xmutex_destroy (&c->mutex); }
static void mulib_prim_notify_ (mulib_prim_core_ *c)
{
  mulib_prim_waiter_ *w;
  for (w = c->waiters; w; w = w->next) if (w->fiber) mulib_fiber_unpark (w->fiber);
  (void)mulib_xcond_broadcast (&c->changed);
}
/* Caller holds core lock before and after. Register under that lock before
 * dropping it, and remove under it, so notifications cannot target dead stacks. */
static int mulib_prim_wait_ (mulib_prim_core_ *c, uint64_t deadline)
{
  mulib_prim_waiter_ w, **link;
  int rc;
  if (deadline != UINT64_MAX && mulib_xclock_ms () >= deadline) return MULIB_CONCURRENCY_TIMEOUT;
  w.fiber = mulib_fiber_current (); w.next = c->waiters; c->waiters = &w;
  if (w.fiber) {
    (void)mulib_xmutex_unlock (&c->mutex);
    rc = mulib_fiber_park_until (deadline);
    (void)mulib_xmutex_lock (&c->mutex);
  } else rc = mulib_xcond_wait_until (&c->changed, &c->mutex, deadline);
  for (link = &c->waiters; *link != &w; link = &(*link)->next) {}
  *link = w.next;
  if (w.fiber) mulib_fiber_clear_wake ();
  return rc == -5 ? MULIB_CONCURRENCY_TIMEOUT : rc ? MULIB_CONCURRENCY_SYSTEM : 0;
}
int mulib_semaphore_create (mulib_semaphore_t **out, size_t initial, size_t maximum)
{
  mulib_semaphore_t *s; int rc;
  if (!out) return MULIB_CONCURRENCY_INVALID;
  *out = NULL;
  if (!maximum || initial > maximum) return MULIB_CONCURRENCY_INVALID;
  s = (mulib_semaphore_t *)calloc (1, sizeof (*s));
  if (!s) return MULIB_CONCURRENCY_NOMEM;
  rc = mulib_prim_init_ (&s->core);
  if (rc) { free (s); return rc; }
  s->count = initial; s->maximum = maximum; *out = s; return 0;
}
int mulib_semaphore_wait (mulib_semaphore_t *s, uint64_t ms)
{
  int rc = 0; uint64_t deadline = mulib_xdeadline (ms);
  if (!s) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&s->core.mutex);
  while (!s->count && !rc) rc = mulib_prim_wait_ (&s->core, deadline);
  if (s->count) { --s->count; rc = 0; }
  (void)mulib_xmutex_unlock (&s->core.mutex); return rc;
}
int mulib_semaphore_post (mulib_semaphore_t *s, size_t n)
{
  int rc = 0;
  if (!s) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&s->core.mutex);
  if (n > s->maximum - s->count) rc = MULIB_CONCURRENCY_OVERFLOW;
  else { s->count += n; if (n) mulib_prim_notify_ (&s->core); }
  (void)mulib_xmutex_unlock (&s->core.mutex); return rc;
}
int mulib_semaphore_destroy (mulib_semaphore_t *s)
{
  if (!s) return 0;
  if (s->core.waiters) return MULIB_CONCURRENCY_BUSY;
  mulib_prim_free_ (&s->core); free (s); return 0;
}
int mulib_mutex_create (mulib_mutex_t **out)
{
  mulib_mutex_t *m; int rc;
  if (!out) return MULIB_CONCURRENCY_INVALID;
  *out = NULL;
  m = (mulib_mutex_t *)malloc (sizeof (*m));
  if (!m) return MULIB_CONCURRENCY_NOMEM;
  rc = mulib_semaphore_create (&m->semaphore, 1, 1);
  if (rc) { free (m); return rc; }
  *out = m; return 0;
}
int mulib_mutex_lock (mulib_mutex_t *m, uint64_t ms)
{ return m ? mulib_semaphore_wait (m->semaphore, ms) : MULIB_CONCURRENCY_INVALID; }
int mulib_mutex_unlock (mulib_mutex_t *m)
{ return m ? mulib_semaphore_post (m->semaphore, 1) : MULIB_CONCURRENCY_INVALID; }
int mulib_mutex_destroy (mulib_mutex_t *m)
{
  int rc;
  if (!m) return 0;
  if (!m->semaphore->count) return MULIB_CONCURRENCY_BUSY;
  rc = mulib_semaphore_destroy (m->semaphore);
  if (!rc) free (m);
  return rc;
}
int mulib_waitgroup_create (mulib_waitgroup_t **out)
{
  mulib_waitgroup_t *g; int rc;
  if (!out) return MULIB_CONCURRENCY_INVALID;
  *out = NULL; g = (mulib_waitgroup_t *)calloc (1, sizeof (*g));
  if (!g) return MULIB_CONCURRENCY_NOMEM;
  rc = mulib_prim_init_ (&g->core);
  if (rc) { free (g); return rc; }
  *out = g; return 0;
}
int mulib_waitgroup_add (mulib_waitgroup_t *g, size_t n)
{
  int rc = 0;
  if (!g) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&g->core.mutex);
  if (n > SIZE_MAX - g->count) rc = MULIB_CONCURRENCY_OVERFLOW;
  else g->count += n;
  (void)mulib_xmutex_unlock (&g->core.mutex); return rc;
}
int mulib_waitgroup_done (mulib_waitgroup_t *g)
{
  int rc = 0;
  if (!g) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&g->core.mutex);
  if (!g->count) rc = MULIB_CONCURRENCY_INVALID;
  else if (!--g->count) mulib_prim_notify_ (&g->core);
  (void)mulib_xmutex_unlock (&g->core.mutex); return rc;
}
int mulib_waitgroup_wait (mulib_waitgroup_t *g, uint64_t ms)
{
  int rc = 0; uint64_t deadline = mulib_xdeadline (ms);
  if (!g) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&g->core.mutex);
  while (g->count && !rc) rc = mulib_prim_wait_ (&g->core, deadline);
  if (!g->count) rc = 0;
  (void)mulib_xmutex_unlock (&g->core.mutex); return rc;
}
int mulib_waitgroup_destroy (mulib_waitgroup_t *g)
{
  if (!g) return 0;
  if (g->count || g->core.waiters) return MULIB_CONCURRENCY_BUSY;
  mulib_prim_free_ (&g->core); free (g); return 0;
}
int mulib_sync_event_create (mulib_sync_event_t **out, int signaled)
{
  mulib_sync_event_t *e; int rc;
  if (!out) return MULIB_CONCURRENCY_INVALID;
  *out = NULL; e = (mulib_sync_event_t *)calloc (1, sizeof (*e));
  if (!e) return MULIB_CONCURRENCY_NOMEM;
  rc = mulib_prim_init_ (&e->core);
  if (rc) { free (e); return rc; }
  e->signaled = !!signaled; *out = e; return 0;
}
int mulib_sync_event_set (mulib_sync_event_t *e)
{
  if (!e) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&e->core.mutex);
  e->signaled = 1; mulib_prim_notify_ (&e->core);
  (void)mulib_xmutex_unlock (&e->core.mutex); return 0;
}
int mulib_sync_event_reset (mulib_sync_event_t *e)
{
  if (!e) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&e->core.mutex); e->signaled = 0;
  (void)mulib_xmutex_unlock (&e->core.mutex); return 0;
}
int mulib_sync_event_wait (mulib_sync_event_t *e, uint64_t ms)
{
  int rc = 0; uint64_t deadline = mulib_xdeadline (ms);
  if (!e) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&e->core.mutex);
  while (!e->signaled && !rc) rc = mulib_prim_wait_ (&e->core, deadline);
  if (e->signaled) rc = 0;
  (void)mulib_xmutex_unlock (&e->core.mutex); return rc;
}
int mulib_sync_event_destroy (mulib_sync_event_t *e)
{
  if (!e) return 0;
  if (e->core.waiters) return MULIB_CONCURRENCY_BUSY;
  mulib_prim_free_ (&e->core); free (e); return 0;
}
int mulib_rwlock_create (mulib_rwlock_t **out)
{
  mulib_rwlock_t *r; int rc;
  if (!out) return MULIB_CONCURRENCY_INVALID;
  *out = NULL; r = (mulib_rwlock_t *)calloc (1, sizeof (*r));
  if (!r) return MULIB_CONCURRENCY_NOMEM;
  rc = mulib_prim_init_ (&r->core);
  if (rc) { free (r); return rc; }
  *out = r; return 0;
}
int mulib_rwlock_read_lock (mulib_rwlock_t *r, uint64_t ms)
{
  int rc = 0; uint64_t deadline = mulib_xdeadline (ms);
  if (!r) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&r->core.mutex);
  while ((r->writer || r->writers_waiting) && !rc) rc = mulib_prim_wait_ (&r->core, deadline);
  if (!r->writer && !r->writers_waiting) {
    if (r->readers == SIZE_MAX) rc = MULIB_CONCURRENCY_OVERFLOW;
    else { ++r->readers; rc = 0; }
  }
  (void)mulib_xmutex_unlock (&r->core.mutex); return rc;
}
int mulib_rwlock_write_lock (mulib_rwlock_t *r, uint64_t ms)
{
  int rc = 0; uint64_t deadline = mulib_xdeadline (ms);
  if (!r) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&r->core.mutex);
  if (r->writers_waiting == SIZE_MAX) {
    (void)mulib_xmutex_unlock (&r->core.mutex); return MULIB_CONCURRENCY_OVERFLOW;
  }
  ++r->writers_waiting;
  while ((r->writer || r->readers) && !rc) rc = mulib_prim_wait_ (&r->core, deadline);
  --r->writers_waiting;
  if (!r->writer && !r->readers) { r->writer = 1; rc = 0; }
  else if (!r->writers_waiting) mulib_prim_notify_ (&r->core);
  (void)mulib_xmutex_unlock (&r->core.mutex); return rc;
}
int mulib_rwlock_read_unlock (mulib_rwlock_t *r)
{
  int rc = 0;
  if (!r) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&r->core.mutex);
  if (!r->readers) rc = MULIB_CONCURRENCY_INVALID;
  else if (!--r->readers) mulib_prim_notify_ (&r->core);
  (void)mulib_xmutex_unlock (&r->core.mutex); return rc;
}
int mulib_rwlock_write_unlock (mulib_rwlock_t *r)
{
  int rc = 0;
  if (!r) return MULIB_CONCURRENCY_INVALID;
  (void)mulib_xmutex_lock (&r->core.mutex);
  if (!r->writer) rc = MULIB_CONCURRENCY_INVALID;
  else { r->writer = 0; mulib_prim_notify_ (&r->core); }
  (void)mulib_xmutex_unlock (&r->core.mutex); return rc;
}
int mulib_rwlock_destroy (mulib_rwlock_t *r)
{
  if (!r) return 0;
  if (r->readers || r->writer || r->writers_waiting || r->core.waiters) return MULIB_CONCURRENCY_BUSY;
  mulib_prim_free_ (&r->core); free (r); return 0;
}
#endif
