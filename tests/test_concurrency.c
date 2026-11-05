#include <stdlib.h>
#include <assert.h>
static long allocation_budget = -1;
static void *test_malloc (size_t size)
{
  if (!allocation_budget) return NULL;
  if (allocation_budget > 0) --allocation_budget;
  return malloc (size);
}
static void *test_calloc (size_t n, size_t size)
{
  if (!allocation_budget) return NULL;
  if (allocation_budget > 0) --allocation_budget;
  return calloc (n, size);
}
#define malloc test_malloc
#define calloc test_calloc
#define MULIB_IMPL_XTHREADS
#define MULIB_IMPL_XATOMICS
#define MULIB_IMPL_FIBER
#define MULIB_IMPL_CHANNEL
#define MULIB_IMPL_CONCURRENCY_PRIM
#include "mulib-xatomics.h"
#include "mulib-channel.h"
#include "mulib-concurrency-prim.h"
#undef malloc
#undef calloc
#include <assert.h>
#include <stdio.h>

#define FOREVER MULIB_XWAIT_FOREVER
static mulib_xatomic_u64_t total = MULIB_XATOMIC_INIT (0);
static mulib_xonce_t once = MULIB_XONCE_INIT;
static mulib_xtss_t key;
static int once_count;
static void once_fn (void) { ++once_count; }
static int atomic_worker (void *arg)
{
  size_t i;
  assert (!mulib_xcall_once (&once, once_fn));
  assert (!mulib_xtss_get (key));
  assert (!mulib_xtss_set (key, arg));
  for (i = 0; i < 10000; ++i) mulib_xatomic_u64_fetch_add (&total, 1);
  assert (mulib_xtss_get (key) == arg);
  return -17;
}
static void test_threads_atomics (void)
{
  mulib_xthread_t threads[4];
  mulib_xatomic_u32_t a = MULIB_XATOMIC_INIT (UINT32_MAX);
  mulib_xatomic_ptr_t p = MULIB_XATOMIC_INIT (NULL);
  uint32_t expected; uint64_t expected64; void *pointer; size_t i; int result;
  mulib_xmutex_t mutex; mulib_xcond_t cond;
  assert (!mulib_xtss_create (&key));
  for (i = 0; i < 4; ++i) assert (!mulib_xthread_create (&threads[i], atomic_worker, &threads[i]));
  for (i = 0; i < 4; ++i) {
    assert (!mulib_xthread_join (threads[i], &result)); assert (result == -17);
  }
  assert (once_count == 1 && mulib_xatomic_u64_load (&total) == 40000);
  assert (!mulib_xtss_get (key)); mulib_xtss_delete (key);
  assert (mulib_xatomic_u32_fetch_add (&a, 1) == UINT32_MAX);
  expected = 1;
  assert (!mulib_xatomic_u32_compare_exchange (&a, &expected, 9) && expected == 0);
  assert (mulib_xatomic_u32_compare_exchange (&a, &expected, 9));
  assert (mulib_xatomic_u32_fetch_sub (&a, 2) == 9);
  assert (mulib_xatomic_u32_fetch_and (&a, 3) == 7);
  assert (mulib_xatomic_u32_fetch_or (&a, 8) == 3);
  assert (mulib_xatomic_u32_fetch_xor (&a, 1) == 11);
  assert (mulib_xatomic_u32_exchange (&a, 99) == 10);
  mulib_xatomic_u32_store (&a, 12); assert (mulib_xatomic_u32_load (&a) == 12);
  expected64 = 0;
  assert (!mulib_xatomic_u64_compare_exchange (&total, &expected64, 1) && expected64 == 40000);
  assert (mulib_xatomic_u64_compare_exchange (&total, &expected64, UINT64_MAX));
  assert (mulib_xatomic_u64_fetch_add (&total, 1) == UINT64_MAX);
  assert (mulib_xatomic_u64_fetch_sub (&total, 1) == 0);
  assert (mulib_xatomic_u64_fetch_and (&total, 3) == UINT64_MAX);
  assert (mulib_xatomic_u64_fetch_or (&total, 8) == 3);
  assert (mulib_xatomic_u64_fetch_xor (&total, 1) == 11);
  assert (mulib_xatomic_u64_exchange (&total, 42) == 10);
  mulib_xatomic_u64_store (&total, 0);
  pointer = &a;
  assert (!mulib_xatomic_ptr_compare_exchange (&p, &pointer, &total) && !pointer);
  assert (mulib_xatomic_ptr_compare_exchange (&p, &pointer, &total));
  assert (mulib_xatomic_ptr_exchange (&p, &a) == &total);
  assert (mulib_xatomic_ptr_load (&p) == &a);
  mulib_xatomic_ptr_store (&p, NULL); mulib_xatomic_fence ();
  assert (!mulib_xatomic_ptr_load (&p));
  assert (!mulib_xmutex_init (&mutex)); assert (!mulib_xcond_init (&cond));
  assert (!mulib_xmutex_trylock (&mutex));
  assert (mulib_xmutex_trylock (&mutex) == MULIB_XTHREAD_BUSY);
  assert (mulib_xcond_wait_until (&cond, &mutex, mulib_xdeadline (2)) == MULIB_XTHREAD_TIMEOUT);
  assert (!mulib_xmutex_unlock (&mutex));
  mulib_xcond_destroy (&cond); mulib_xmutex_destroy (&mutex);
}

static void test_channel_edges (void)
{
  mulib_channel_t *c = NULL; int in = 7, out = -1; size_t chosen;
  mulib_channel_case_t cases[3];
  assert (mulib_channel_create (&c, 0, 1) == MULIB_CHANNEL_INVALID && !c);
  assert (mulib_channel_create (&c, 2, SIZE_MAX) == MULIB_CHANNEL_INVALID);
  assert (!mulib_channel_create (&c, sizeof (int), 2));
  assert (!mulib_channel_send (mulib_channel_sender (c), &in, 0));
  in = 8; assert (!mulib_channel_send (mulib_channel_sender (c), &in, 0));
  assert (mulib_channel_send (mulib_channel_sender (c), &in, 0) == MULIB_CHANNEL_TIMEOUT);
  assert (!mulib_channel_receive (mulib_channel_receiver (c), &out, 0) && out == 7);
  in = 9; assert (!mulib_channel_send (mulib_channel_sender (c), &in, 0));
  assert (!mulib_channel_close (c));
  assert (mulib_channel_close (c) == MULIB_CHANNEL_CLOSED);
  assert (mulib_channel_send (mulib_channel_sender (c), &in, 0) == MULIB_CHANNEL_CLOSED);
  assert (!mulib_channel_receive (mulib_channel_receiver (c), &out, 0) && out == 8);
  assert (!mulib_channel_receive (mulib_channel_receiver (c), &out, 0) && out == 9);
  assert (mulib_channel_receive (mulib_channel_receiver (c), &out, 0) == MULIB_CHANNEL_CLOSED && out == 0);
  cases[0] = mulib_channel_receive_case (mulib_channel_receiver (NULL), NULL);
  cases[1] = mulib_channel_receive_case (mulib_channel_receiver (c), &out);
  assert (mulib_channel_select (cases, 2, 1, 0, &chosen) == MULIB_CHANNEL_CLOSED && chosen == 1);
  assert (!mulib_channel_destroy (c));
  assert (mulib_channel_select (NULL, 0, 1, FOREVER, &chosen) == MULIB_CHANNEL_DEFAULT && chosen == SIZE_MAX);
  assert (mulib_channel_select (cases, 1, 0, 2, &chosen) == MULIB_CHANNEL_TIMEOUT && chosen == SIZE_MAX);
  assert (!mulib_channel_create (&c, sizeof (int), 0));
  cases[0] = mulib_channel_send_case (mulib_channel_sender (c), &in);
  cases[1] = mulib_channel_receive_case (mulib_channel_receiver (c), &out);
  cases[2] = cases[1];
  out = -1;
  assert (mulib_channel_select (cases, 3, 0, 2, &chosen) == MULIB_CHANNEL_TIMEOUT && out == -1);
  /* A select must not match its own send and receive, including duplicates. */
  assert (!mulib_channel_destroy (c));
}

typedef struct pipeline {
  mulib_channel_t *a, *b;
  int sum, count;
} pipeline;
static void producer (void *arg)
{
  pipeline *p = (pipeline *)arg; int i;
  for (i = 1; i <= p->count; ++i) {
    mulib_channel_case_t cases[2]; size_t chosen;
    cases[0] = mulib_channel_send_case (mulib_channel_sender (p->a), &i);
    cases[1] = mulib_channel_send_case (mulib_channel_sender (p->b), &i);
    assert (!mulib_channel_select (cases, 2, 0, FOREVER, &chosen) && chosen < 2);
  }
  assert (!mulib_channel_close (p->a)); assert (!mulib_channel_close (p->b));
}
static void consumer (void *arg)
{
  pipeline *p = (pipeline *)arg; int value = 0, open = 2;
  mulib_channel_case_t cases[2]; size_t chosen;
  cases[0] = mulib_channel_receive_case (mulib_channel_receiver (p->a), &value);
  cases[1] = mulib_channel_receive_case (mulib_channel_receiver (p->b), &value);
  while (open) {
    int rc = mulib_channel_select (cases, 2, 0, FOREVER, &chosen);
    assert (chosen < 2);
    if (rc == MULIB_CHANNEL_CLOSED) { cases[chosen].channel = NULL; --open; }
    else { assert (!rc); p->sum += value; }
  }
}
static int native_producer (void *p) { producer (p); return 0; }
static int native_consumer (void *p) { consumer (p); return 0; }
static void test_native_pipeline (size_t capacity)
{
  pipeline p; mulib_xthread_t sender, receiver;
  p.sum = 0; p.count = 2000;
  assert (!mulib_channel_create (&p.a, sizeof (int), capacity));
  assert (!mulib_channel_create (&p.b, sizeof (int), capacity));
  assert (!mulib_xthread_create (&receiver, native_consumer, &p));
  assert (!mulib_xthread_create (&sender, native_producer, &p));
  assert (!mulib_xthread_join (sender, NULL)); assert (!mulib_xthread_join (receiver, NULL));
  assert (p.sum == p.count * (p.count + 1) / 2);
  assert (!mulib_channel_destroy (p.a)); assert (!mulib_channel_destroy (p.b));
}
static void test_pipeline (size_t capacity, int mode)
{
  pipeline p; mulib_fiber_scheduler_t *s; mulib_xthread_t t;
  p.sum = 0; p.count = 2000;
  assert (!mulib_channel_create (&p.a, sizeof (int), capacity));
  assert (!mulib_channel_create (&p.b, sizeof (int), capacity));
  assert (!mulib_fiber_scheduler_create (&s));
  if (mode == 0) {
    assert (!mulib_fiber_spawn (s, consumer, &p, 0, NULL));
    assert (!mulib_fiber_spawn (s, producer, &p, 0, NULL));
  } else if (mode == 1) {
    assert (!mulib_fiber_spawn (s, consumer, &p, 0, NULL));
    assert (!mulib_xthread_create (&t, native_producer, &p));
  } else {
    assert (!mulib_fiber_spawn (s, producer, &p, 0, NULL));
    assert (!mulib_xthread_create (&t, native_consumer, &p));
  }
  assert (mulib_fiber_scheduler_destroy (s) == MULIB_FIBER_BUSY);
  assert (!mulib_fiber_run (s));
  if (mode) assert (!mulib_xthread_join (t, NULL));
  assert (p.sum == p.count * (p.count + 1) / 2);
  assert (!mulib_fiber_scheduler_destroy (s));
  assert (!mulib_channel_destroy (p.a)); assert (!mulib_channel_destroy (p.b));
}
typedef struct prim_test {
  mulib_mutex_t *mutex;
  mulib_semaphore_t *semaphore;
  mulib_waitgroup_t *group;
  mulib_sync_event_t *event;
  mulib_rwlock_t *rw;
  int count;
} prim_test;
static void prim_worker (void *arg)
{
  prim_test *p = (prim_test *)arg; int i;
  assert (!mulib_sync_event_wait (p->event, FOREVER));
  assert (!mulib_semaphore_wait (p->semaphore, FOREVER));
  for (i = 0; i < 100; ++i) {
    int old;
    assert (!mulib_mutex_lock (p->mutex, FOREVER));
    old = p->count;
    mulib_fiber_yield ();
    p->count = old + 1;
    assert (!mulib_mutex_unlock (p->mutex));
  }
  assert (!mulib_semaphore_post (p->semaphore, 1));
  assert (!mulib_waitgroup_done (p->group));
}
static void prim_controller (void *arg)
{
  prim_test *p = (prim_test *)arg;
  mulib_fiber_sleep (2);
  assert (!mulib_sync_event_set (p->event));
  assert (!mulib_waitgroup_wait (p->group, FOREVER));
  assert (p->count == 800);
}
static int native_prim_worker (void *arg) { prim_worker (arg); return 0; }
static void test_primitives (void)
{
  prim_test p; mulib_fiber_scheduler_t *s; mulib_xthread_t t; size_t i;
  p.count = 0;
  assert (!mulib_mutex_create (&p.mutex));
  assert (!mulib_semaphore_create (&p.semaphore, 2, 2));
  assert (mulib_semaphore_post (p.semaphore, 1) == MULIB_CONCURRENCY_OVERFLOW);
  assert (!mulib_waitgroup_create (&p.group));
  assert (mulib_waitgroup_done (p.group) == MULIB_CONCURRENCY_INVALID);
  assert (!mulib_waitgroup_add (p.group, SIZE_MAX));
  assert (mulib_waitgroup_add (p.group, 1) == MULIB_CONCURRENCY_OVERFLOW);
  /* Reset only under exclusive ownership for this boundary test. */
  p.group->count = 0;
  assert (!mulib_waitgroup_add (p.group, 8));
  assert (!mulib_sync_event_create (&p.event, 0));
  assert (mulib_sync_event_wait (p.event, 0) == MULIB_CONCURRENCY_TIMEOUT);
  assert (!mulib_rwlock_create (&p.rw));
  assert (!mulib_rwlock_read_lock (p.rw, 0));
  assert (!mulib_rwlock_read_lock (p.rw, 0));
  assert (mulib_rwlock_write_lock (p.rw, 0) == MULIB_CONCURRENCY_TIMEOUT);
  assert (mulib_rwlock_destroy (p.rw) == MULIB_CONCURRENCY_BUSY);
  assert (!mulib_rwlock_read_unlock (p.rw)); assert (!mulib_rwlock_read_unlock (p.rw));
  assert (!mulib_rwlock_write_lock (p.rw, 0));
  assert (mulib_rwlock_read_lock (p.rw, 0) == MULIB_CONCURRENCY_TIMEOUT);
  assert (!mulib_rwlock_write_unlock (p.rw));
  assert (!mulib_fiber_scheduler_create (&s));
  for (i = 0; i < 7; ++i) assert (!mulib_fiber_spawn (s, prim_worker, &p, 0, NULL));
  assert (!mulib_xthread_create (&t, native_prim_worker, &p));
  assert (!mulib_fiber_spawn (s, prim_controller, &p, 0, NULL));
  assert (!mulib_fiber_run (s)); assert (!mulib_xthread_join (t, NULL));
  assert (!mulib_fiber_scheduler_destroy (s));
  assert (!mulib_sync_event_reset (p.event));
  assert (mulib_sync_event_wait (p.event, 1) == MULIB_CONCURRENCY_TIMEOUT);
  assert (!mulib_sync_event_destroy (p.event)); assert (!mulib_waitgroup_destroy (p.group));
  assert (!mulib_semaphore_destroy (p.semaphore)); assert (!mulib_mutex_destroy (p.mutex));
  assert (!mulib_rwlock_destroy (p.rw));
}
static void park_test (void *arg)
{
  int *steps = (int *)arg;
  mulib_fiber_t *self = mulib_fiber_current ();
  assert (self);
  mulib_fiber_unpark (self);
  assert (!mulib_fiber_park_until (FOREVER));
  assert (mulib_fiber_park_until (mulib_xdeadline (2)) == MULIB_FIBER_TIMEOUT);
  ++*steps; mulib_fiber_yield (); ++*steps;
}
static void test_fibers (void)
{
  mulib_fiber_scheduler_t *s; int steps = 0;
  assert (!mulib_fiber_current ());
  assert (mulib_fiber_park_until (0) == MULIB_FIBER_INVALID);
  assert (!mulib_fiber_scheduler_create (&s));
  assert (!mulib_fiber_run (s));
  assert (mulib_fiber_spawn (s, park_test, &steps, 1, NULL) == MULIB_FIBER_INVALID);
  assert (!mulib_fiber_spawn (s, park_test, &steps, 0, NULL));
  assert (!mulib_fiber_run (s) && steps == 2);
  assert (!mulib_fiber_spawn (s, park_test, &steps, 0, NULL));
  assert (!mulib_fiber_run (s) && steps == 4);
  assert (!mulib_fiber_scheduler_destroy (s));
}
typedef struct close_test {
  mulib_channel_t *a, *b;
  int waiting_send, result, output;
} close_test;
static void close_waiter (void *arg)
{
  close_test *p = (close_test *)arg; int value = 123; size_t chosen;
  mulib_channel_case_t cases[3];
  if (p->waiting_send) {
    cases[0] = mulib_channel_send_case (mulib_channel_sender (p->a), &value);
    cases[1] = mulib_channel_send_case (mulib_channel_sender (p->b), &value);
  } else {
    cases[0] = mulib_channel_receive_case (mulib_channel_receiver (p->a), &p->output);
    cases[1] = mulib_channel_receive_case (mulib_channel_receiver (p->b), &p->output);
  }
  cases[2] = cases[0];
  p->result = mulib_channel_select (cases, 3, 0, FOREVER, &chosen);
  assert (p->result == MULIB_CHANNEL_CLOSED && chosen == 1);
}
static void closer (void *arg)
{
  close_test *p = (close_test *)arg;
  /* Scheduler starts waiter first, so these checks deterministically see it. */
  assert (mulib_channel_destroy (p->a) == MULIB_CHANNEL_BUSY);
  assert (!mulib_channel_close (p->b));
  assert (mulib_channel_destroy (p->b) == MULIB_CHANNEL_BUSY);
}
static void test_close_waiters (void)
{
  close_test p; mulib_fiber_scheduler_t *s; int sending;
  for (sending = 0; sending < 2; ++sending) {
    p.waiting_send = sending; p.result = 0; p.output = -1;
    assert (!mulib_channel_create (&p.a, sizeof (int), 0));
    assert (!mulib_channel_create (&p.b, sizeof (int), 0));
    assert (!mulib_fiber_scheduler_create (&s));
    assert (!mulib_fiber_spawn (s, close_waiter, &p, 0, NULL));
    assert (!mulib_fiber_spawn (s, closer, &p, 0, NULL));
    assert (!mulib_fiber_run (s));
    assert (p.output == (sending ? -1 : 0));
    assert (!mulib_fiber_scheduler_destroy (s));
    assert (!mulib_channel_destroy (p.a)); assert (!mulib_channel_destroy (p.b));
  }
}
static void no_work (void *arg) { (void)arg; }
typedef struct rw_test { mulib_rwlock_t *lock; int phase; } rw_test;
static void rw_reader (void *arg)
{
  rw_test *p = (rw_test *)arg;
  assert (!mulib_rwlock_read_lock (p->lock, FOREVER));
  p->phase = 1;
  mulib_fiber_yield ();
  assert (p->phase == 1);
  assert (!mulib_rwlock_read_unlock (p->lock));
  mulib_fiber_yield ();
  assert (!mulib_rwlock_read_lock (p->lock, FOREVER));
  assert (p->phase == 3);
  assert (!mulib_rwlock_read_unlock (p->lock));
}
static void rw_writer (void *arg)
{
  rw_test *p = (rw_test *)arg;
  assert (!mulib_rwlock_write_lock (p->lock, FOREVER));
  assert (p->phase == 1); p->phase = 2;
  mulib_fiber_yield ();
  p->phase = 3;
  assert (!mulib_rwlock_write_unlock (p->lock));
}
static void rw_try_reader (void *arg)
{
  rw_test *p = (rw_test *)arg;
  /* The first reader still holds the lock, but the writer is now queued. */
  assert (p->phase == 1);
  assert (mulib_rwlock_read_lock (p->lock, 0) == MULIB_CONCURRENCY_TIMEOUT);
}
static void test_rw_contention (void)
{
  rw_test p; mulib_fiber_scheduler_t *s;
  p.phase = 0; assert (!mulib_rwlock_create (&p.lock));
  assert (!mulib_fiber_scheduler_create (&s));
  assert (!mulib_fiber_spawn (s, rw_reader, &p, 0, NULL));
  assert (!mulib_fiber_spawn (s, rw_writer, &p, 0, NULL));
  assert (!mulib_fiber_spawn (s, rw_try_reader, &p, 0, NULL));
  assert (!mulib_fiber_run (s) && p.phase == 3);
  assert (!mulib_fiber_scheduler_destroy (s)); assert (!mulib_rwlock_destroy (p.lock));
}
static void test_allocation_failures (void)
{
  mulib_channel_t *c; mulib_fiber_scheduler_t *s; mulib_mutex_t *m;
  mulib_semaphore_t *sem; mulib_waitgroup_t *g; mulib_sync_event_t *e; mulib_rwlock_t *r;
  mulib_fiber_t *f; mulib_channel_case_t item; size_t chosen; int value = 1;
  long budget;
  for (budget = 0; budget < 2; ++budget) {
    allocation_budget = budget;
    assert (mulib_channel_create (&c, sizeof (int), 2) == MULIB_CHANNEL_NOMEM && !c);
    allocation_budget = budget;
    assert (mulib_mutex_create (&m) == MULIB_CONCURRENCY_NOMEM && !m);
  }
  allocation_budget = 0;
  assert (mulib_fiber_scheduler_create (&s) == MULIB_FIBER_NOMEM && !s);
  assert (mulib_semaphore_create (&sem, 0, 1) == MULIB_CONCURRENCY_NOMEM && !sem);
  assert (mulib_waitgroup_create (&g) == MULIB_CONCURRENCY_NOMEM && !g);
  assert (mulib_sync_event_create (&e, 0) == MULIB_CONCURRENCY_NOMEM && !e);
  assert (mulib_rwlock_create (&r) == MULIB_CONCURRENCY_NOMEM && !r);
  allocation_budget = -1;
  assert (!mulib_fiber_scheduler_create (&s));
  allocation_budget = 0;
  assert (mulib_fiber_spawn (s, no_work, NULL, 0, &f) == MULIB_FIBER_NOMEM && !f);
#if !defined(_WIN32)
  allocation_budget = 1;
  assert (mulib_fiber_spawn (s, no_work, NULL, 0, &f) == MULIB_FIBER_NOMEM && !f);
#endif
  allocation_budget = -1;
  assert (!mulib_fiber_run (s)); assert (!mulib_fiber_scheduler_destroy (s));
  assert (!mulib_channel_create (&c, sizeof (int), 0));
  item = mulib_channel_send_case (mulib_channel_sender (c), &value);
  allocation_budget = 0;
  assert (mulib_channel_select (&item, 1, 0, FOREVER, &chosen) == MULIB_CHANNEL_NOMEM);
  assert (chosen == SIZE_MAX);
  allocation_budget = -1;
  assert (!mulib_channel_destroy (c));
}
int main (int argc, char **argv)
{
  int mode;
  test_threads_atomics (); test_channel_edges ();
  test_native_pipeline (0); test_native_pipeline (3);
  mulib_xthread_sleep (1);
  if (argc == 2 && !strcmp (argv[1], "--native")) { puts ("native concurrency: ok"); return 0; }
  test_fibers ();
  for (mode = 0; mode < 3; ++mode) { test_pipeline (0, mode); test_pipeline (3, mode); }
  test_primitives (); test_close_waiters (); test_rw_contention (); test_allocation_failures ();
  puts ("concurrency: ok"); return 0;
}
