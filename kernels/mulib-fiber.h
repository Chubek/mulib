/**
 * @file mulib-fiber.h
 * @brief Stackful cooperative fibers with parking, wakeups and timed sleep.
 * Define MULIB_IMPL_FIBER and MULIB_IMPL_XTHREADS once, independently.
 */
#ifndef MULIB_FIBER_H
#define MULIB_FIBER_H
#include "mulib-xthreads.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fiber mulib_fiber_t;
typedef struct mulib_fiber_scheduler mulib_fiber_scheduler_t;
typedef void (*mulib_fiber_fn)(void *);
enum {
  MULIB_FIBER_OK = 0, MULIB_FIBER_INVALID = -1, MULIB_FIBER_NOMEM = -2,
  MULIB_FIBER_SYSTEM = -3, MULIB_FIBER_BUSY = -4, MULIB_FIBER_TIMEOUT = -5
};
/** Allocate a scheduler. Spawn/run/destroy belong to one native thread. */
int mulib_fiber_scheduler_create (mulib_fiber_scheduler_t **out);
/** Run until every fiber finishes; sleeps in the OS when no fiber is ready.
 * No nested runs; unpark may be called by other native threads. */
int mulib_fiber_run (mulib_fiber_scheduler_t *);
/** Free a quiescent scheduler and all handles; BUSY if any fiber is unfinished. */
int mulib_fiber_scheduler_destroy (mulib_fiber_scheduler_t *);
/** Spawn a callback with a private stack (0 selects 256 KiB). Optional out is
 * borrowed until scheduler destruction. Argument stays caller-owned. */
int mulib_fiber_spawn (mulib_fiber_scheduler_t *, mulib_fiber_fn, void *,
                       size_t stack_size, mulib_fiber_t **out);
/** Return the running fiber, or NULL outside a fiber callback. */
mulib_fiber_t *mulib_fiber_current (void);
/** Cooperatively yield; outside a fiber, yield the native thread. */
void mulib_fiber_yield (void);
/** Park until unpark or absolute UTC deadline. Outside a fiber returns INVALID.
 * A one-bit wake permit prevents a wake-before-park race. */
int mulib_fiber_park_until (uint64_t deadline);
/** Set the wake permit and make a parked fiber runnable. Thread-safe; repeated
 * wakes coalesce. The handle must remain alive throughout this call. */
void mulib_fiber_unpark (mulib_fiber_t *);
/** Consume any unused wake permit on the current fiber. For wait adapters only,
 * after their notification source has been unregistered. */
void mulib_fiber_clear_wake (void);
/** Suspend only this fiber (or sleep the native thread outside a fiber). */
void mulib_fiber_sleep (uint64_t milliseconds);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_FIBER) && !defined(MULIB_FIBER_IMPLEMENTED)
#define MULIB_FIBER_IMPLEMENTED
#include <stdlib.h>
#if !defined(_WIN32)
#include <ucontext.h>
#if defined(__SANITIZE_ADDRESS__)
#define MULIB_FIBER_ASAN_ 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MULIB_FIBER_ASAN_ 1
#endif
#endif
#endif
#ifdef MULIB_FIBER_ASAN_
#include <sanitizer/common_interface_defs.h>
#endif
#if defined(__cplusplus)
#define MULIB_FIBER_TLS_ thread_local
#elif defined(_MSC_VER)
#define MULIB_FIBER_TLS_ __declspec(thread)
#else
#define MULIB_FIBER_TLS_ _Thread_local
#endif
enum { MULIB_FIBER_READY_, MULIB_FIBER_RUNNING_, MULIB_FIBER_PARKED_, MULIB_FIBER_DONE_ };
struct mulib_fiber {
  mulib_fiber_scheduler_t *scheduler;
  mulib_fiber_t *next;
  mulib_fiber_fn fn;
  void *arg;
  int state, permit;
  uint64_t deadline;
#ifdef MULIB_FIBER_ASAN_
  void *asan_fake_stack;
#endif
#if defined(_WIN32)
  void *context;
#else
  ucontext_t context;
  void *stack;
#endif
};
struct mulib_fiber_scheduler {
  mulib_xmutex_t mutex;
  mulib_xcond_t changed;
  mulib_fiber_t *head, *tail, *cursor, *current;
  size_t live;
#ifdef MULIB_FIBER_ASAN_
  void *asan_fake_stack;
  const void *asan_stack_bottom;
  size_t asan_stack_size;
#endif
#if defined(_WIN32)
  void *context;
#else
  ucontext_t context;
#endif
};
static MULIB_FIBER_TLS_ mulib_fiber_scheduler_t *mulib_fiber_scheduler_current_;
#undef MULIB_FIBER_TLS_
static void mulib_fiber_switch_out_ (mulib_fiber_t *f, int finished)
{
  (void)finished;
#if defined(_WIN32)
  SwitchToFiber (f->scheduler->context);
#else
  /* Valid contexts and allocated stacks make swapcontext failure unrecoverable. */
#ifdef MULIB_FIBER_ASAN_
  __sanitizer_start_switch_fiber (finished ? NULL : &f->asan_fake_stack,
                                  f->scheduler->asan_stack_bottom, f->scheduler->asan_stack_size);
#endif
  if (swapcontext (&f->context, &f->scheduler->context)) abort ();
#ifdef MULIB_FIBER_ASAN_
  __sanitizer_finish_switch_fiber (f->asan_fake_stack, &f->scheduler->asan_stack_bottom,
                                   &f->scheduler->asan_stack_size);
#endif
#endif
}
#if defined(_WIN32)
static VOID WINAPI mulib_fiber_entry_ (void *arg)
#else
static void mulib_fiber_entry_ (void)
#endif
{
  mulib_fiber_t *f = mulib_fiber_scheduler_current_->current;
  mulib_fiber_scheduler_t *s = f->scheduler;
#ifdef MULIB_FIBER_ASAN_
  __sanitizer_finish_switch_fiber (f->asan_fake_stack, &s->asan_stack_bottom, &s->asan_stack_size);
#endif
#if defined(_WIN32)
  (void)arg;
#endif
  f->fn (f->arg);
  (void)mulib_xmutex_lock (&s->mutex);
  f->state = MULIB_FIBER_DONE_; --s->live;
  (void)mulib_xmutex_unlock (&s->mutex);
  mulib_fiber_switch_out_ (f, 1);
  abort ();
}
int mulib_fiber_scheduler_create (mulib_fiber_scheduler_t **out)
{
  mulib_fiber_scheduler_t *s;
  if (!out) return MULIB_FIBER_INVALID;
  *out = NULL;
  s = (mulib_fiber_scheduler_t *)calloc (1, sizeof (*s));
  if (!s) return MULIB_FIBER_NOMEM;
  if (mulib_xmutex_init (&s->mutex)) { free (s); return MULIB_FIBER_SYSTEM; }
  if (mulib_xcond_init (&s->changed)) {
    mulib_xmutex_destroy (&s->mutex); free (s); return MULIB_FIBER_SYSTEM;
  }
  *out = s; return 0;
}
#if !defined(_WIN32)
static int mulib_fiber_context_init_ (mulib_fiber_t *f, size_t size)
{
  if (getcontext (&f->context)) return MULIB_FIBER_SYSTEM;
  f->context.uc_stack.ss_sp = f->stack;
  f->context.uc_stack.ss_size = size;
  f->context.uc_link = NULL;
  makecontext (&f->context, mulib_fiber_entry_, 0);
  return 0;
}
#endif
int mulib_fiber_spawn (mulib_fiber_scheduler_t *s, mulib_fiber_fn fn, void *arg,
                      size_t stack_size, mulib_fiber_t **out)
{
  mulib_fiber_t *f;
  if (out) *out = NULL;
  if (!s || !fn || (stack_size && stack_size < 16384)) return MULIB_FIBER_INVALID;
  if (!stack_size) stack_size = 256 * 1024;
  f = (mulib_fiber_t *)calloc (1, sizeof (*f));
  if (!f) return MULIB_FIBER_NOMEM;
  f->scheduler = s; f->fn = fn; f->arg = arg;
#if defined(_WIN32)
  f->context = CreateFiber (stack_size, mulib_fiber_entry_, NULL);
  if (!f->context) { free (f); return MULIB_FIBER_NOMEM; }
#else
  f->stack = malloc (stack_size);
  if (!f->stack) { free (f); return MULIB_FIBER_NOMEM; }
  if (mulib_fiber_context_init_ (f, stack_size)) {
    free (f->stack); free (f); return MULIB_FIBER_SYSTEM;
  }
#endif
  (void)mulib_xmutex_lock (&s->mutex);
  if (s->tail) s->tail->next = f; else s->head = f;
  s->tail = f; ++s->live;
  (void)mulib_xmutex_unlock (&s->mutex);
  if (out) *out = f;
  return 0;
}
mulib_fiber_t *mulib_fiber_current (void)
{ return mulib_fiber_scheduler_current_ ? mulib_fiber_scheduler_current_->current : NULL; }
int mulib_fiber_run (mulib_fiber_scheduler_t *s)
{
#if defined(_WIN32)
  int converted = 0;
#endif
  if (!s || mulib_fiber_scheduler_current_) return MULIB_FIBER_INVALID;
#if defined(_WIN32)
  if (IsThreadAFiber ()) s->context = GetCurrentFiber ();
  else {
    s->context = ConvertThreadToFiber (NULL);
    if (!s->context) return MULIB_FIBER_SYSTEM;
    converted = 1;
  }
#endif
  mulib_fiber_scheduler_current_ = s;
  (void)mulib_xmutex_lock (&s->mutex);
  while (s->live) {
    mulib_fiber_t *f, *start;
    uint64_t now = mulib_xclock_ms (), deadline = UINT64_MAX;
    start = s->cursor && s->cursor->next ? s->cursor->next : s->head;
    f = start;
    do {
      if (f->state == MULIB_FIBER_PARKED_) {
        if (f->deadline <= now && f->deadline != UINT64_MAX) f->state = MULIB_FIBER_READY_;
        else if (f->deadline < deadline) deadline = f->deadline;
      }
      if (f->state == MULIB_FIBER_READY_) break;
      f = f->next ? f->next : s->head;
    } while (f != start);
    if (f->state != MULIB_FIBER_READY_) {
      (void)mulib_xcond_wait_until (&s->changed, &s->mutex, deadline);
      continue;
    }
    s->cursor = s->current = f; f->state = MULIB_FIBER_RUNNING_;
    (void)mulib_xmutex_unlock (&s->mutex);
#if defined(_WIN32)
    SwitchToFiber (f->context);
#else
#ifdef MULIB_FIBER_ASAN_
    __sanitizer_start_switch_fiber (&s->asan_fake_stack, f->stack, f->context.uc_stack.ss_size);
#endif
    if (swapcontext (&s->context, &f->context)) abort ();
#ifdef MULIB_FIBER_ASAN_
    __sanitizer_finish_switch_fiber (s->asan_fake_stack, NULL, NULL);
#endif
#endif
    (void)mulib_xmutex_lock (&s->mutex);
    s->current = NULL;
    if (f->state == MULIB_FIBER_DONE_) {
#if defined(_WIN32)
      DeleteFiber (f->context); f->context = NULL;
#else
      free (f->stack); f->stack = NULL;
#endif
    }
  }
  (void)mulib_xmutex_unlock (&s->mutex);
  mulib_fiber_scheduler_current_ = NULL;
#if defined(_WIN32)
  if (converted && !ConvertFiberToThread ()) return MULIB_FIBER_SYSTEM;
#endif
  return 0;
}
void mulib_fiber_yield (void)
{
  mulib_fiber_t *f = mulib_fiber_current ();
  if (!f) { mulib_xthread_yield (); return; }
  (void)mulib_xmutex_lock (&f->scheduler->mutex);
  f->state = MULIB_FIBER_READY_;
  (void)mulib_xmutex_unlock (&f->scheduler->mutex);
  mulib_fiber_switch_out_ (f, 0);
}
int mulib_fiber_park_until (uint64_t deadline)
{
  mulib_fiber_t *f = mulib_fiber_current ();
  int rc = 0;
  if (!f) return MULIB_FIBER_INVALID;
  (void)mulib_xmutex_lock (&f->scheduler->mutex);
  if (!f->permit && (deadline == UINT64_MAX || deadline > mulib_xclock_ms ())) {
    f->deadline = deadline; f->state = MULIB_FIBER_PARKED_;
    (void)mulib_xmutex_unlock (&f->scheduler->mutex);
    mulib_fiber_switch_out_ (f, 0);
    (void)mulib_xmutex_lock (&f->scheduler->mutex);
  }
  if (!f->permit) rc = MULIB_FIBER_TIMEOUT;
  f->permit = 0;
  (void)mulib_xmutex_unlock (&f->scheduler->mutex);
  return rc;
}
void mulib_fiber_unpark (mulib_fiber_t *f)
{
  if (!f) return;
  (void)mulib_xmutex_lock (&f->scheduler->mutex);
  if (f->state != MULIB_FIBER_DONE_) {
    f->permit = 1;
    if (f->state == MULIB_FIBER_PARKED_) f->state = MULIB_FIBER_READY_;
    (void)mulib_xcond_signal (&f->scheduler->changed);
  }
  (void)mulib_xmutex_unlock (&f->scheduler->mutex);
}
void mulib_fiber_clear_wake (void)
{
  mulib_fiber_t *f = mulib_fiber_current ();
  if (!f) return;
  (void)mulib_xmutex_lock (&f->scheduler->mutex);
  f->permit = 0;
  (void)mulib_xmutex_unlock (&f->scheduler->mutex);
}
void mulib_fiber_sleep (uint64_t ms)
{
  uint64_t deadline;
  if (!mulib_fiber_current ()) { mulib_xthread_sleep (ms); return; }
  if (!ms) { mulib_fiber_yield (); return; }
  deadline = mulib_xdeadline (ms);
  while (mulib_fiber_park_until (deadline) != MULIB_FIBER_TIMEOUT) {}
}
int mulib_fiber_scheduler_destroy (mulib_fiber_scheduler_t *s)
{
  mulib_fiber_t *f;
  if (!s) return 0;
  if (s->live || mulib_fiber_scheduler_current_ == s) return MULIB_FIBER_BUSY;
  f = s->head;
  while (f) { mulib_fiber_t *next = f->next; free (f); f = next; }
  mulib_xcond_destroy (&s->changed); mulib_xmutex_destroy (&s->mutex); free (s);
  return 0;
}
#endif
