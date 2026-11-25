/**
 * @file mulib-evtsched.h
 * @brief Stable bucketed event scheduling with absolute deadlines in seconds.
 * Define MULIB_IMPL_EVTSCHED in one translation unit. No MuLIB dependencies.
 * Based on Lykron scheduler.c (Chubak Bidpaa, MIT; see module manual).
 */
#ifndef MULIB_EVTSCHED_H
#define MULIB_EVTSCHED_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_evtsched_bucket mulib_evtsched_bucket_t;
  /** Caller-owned intrusive notice. Zero-initialize. Only data is
   * caller-writable. */
  typedef struct mulib_evtsched_notice
  {
    int64_t deadline;
    void *data;
    struct mulib_evtsched_notice *prev, *next;
    mulib_evtsched_bucket_t *bucket;
  } mulib_evtsched_notice_t;

  /** Internal bucket links. Keep the scheduler and queued notices at stable
   * addresses. */
  struct mulib_evtsched_bucket
  {
    struct mulib_evtsched *owner;
    mulib_evtsched_bucket_t *prev, *next;
    mulib_evtsched_notice_t *first, *last;
    size_t count;
  };

  /** Zero-initialize before first use. All operations are single-threaded. */
  typedef struct mulib_evtsched
  {
    mulib_evtsched_bucket_t first;
  } mulib_evtsched_t;

  /** Insert or reschedule at an absolute deadline, after existing ties.
   * Returns 0 on success, -1 with EINVAL for invalid arguments or foreign
   * ownership. */
  int mulib_evtsched_schedule (mulib_evtsched_t *, mulib_evtsched_notice_t *,
                               int64_t);
  /** Postpone a queued notice relative to its deadline. Errors preserve its
   * position. */
  int mulib_evtsched_hold (mulib_evtsched_t *, mulib_evtsched_notice_t *,
                           int64_t delay);
  /** Detach a queued notice. Returns 0 on success or -1 with EINVAL. */
  int mulib_evtsched_cancel (mulib_evtsched_t *, mulib_evtsched_notice_t *);
  /** Borrow the earliest notice, or NULL for an empty or NULL scheduler. */
  mulib_evtsched_notice_t *mulib_evtsched_peek (const mulib_evtsched_t *);
  /** Remove and return the earliest notice, or NULL for an empty or NULL
   * scheduler. */
  mulib_evtsched_notice_t *mulib_evtsched_pop (mulib_evtsched_t *);
  /** Detach all pending notices and release bucket storage. Reusable
   * afterward. */
  void mulib_evtsched_clear (mulib_evtsched_t *);
  /** Receives a detached notice and the observed time. May requeue it for
   * recurrence. */
  typedef void (*mulib_evtsched_fn) (mulib_evtsched_notice_t *, int64_t now,
                                     void *context);
  /** Dispatch at most one due notice against a supplied clock. 1 ran, 0 idle,
   * -1 invalid. */
  int mulib_evtsched_dispatch (mulib_evtsched_t *, int64_t now,
                               mulib_evtsched_fn, void *);
  /** Linux realtime timerfd. Wait and dispatch one notice. 1 ran, 0 empty,
   * -1 with errno on failure. Other platforms report ENOSYS for nonempty
   * queues. */
  int mulib_evtsched_wait_one (mulib_evtsched_t *, mulib_evtsched_fn, void *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_EVTSCHED) && !defined(MULIB_EVTSCHED_IMPLEMENTED)
#define MULIB_EVTSCHED_IMPLEMENTED
#include <errno.h>
#include <stdlib.h>
#ifdef __linux__
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#endif

/* Splitting only accelerates insertion. If allocation fails, the existing
 * sorted bucket remains fully usable. Split by population even for equal keys.
 */
static void
mulib_evtsched_split (mulib_evtsched_bucket_t *left)
{
  mulib_evtsched_bucket_t *right;
  mulib_evtsched_notice_t *node;
  size_t moved;
  if (left->count <= 32)
    return;
  right = (mulib_evtsched_bucket_t *)malloc (sizeof *right);
  if (!right)
    return;
  moved = left->count / 2;
  node = left->last;
  right->last = node;
  for (size_t i = 0; i < moved; ++i)
    {
      node->bucket = right;
      node = node->prev;
    }
  right->first = node->next;
  right->first->prev = NULL;
  left->last = node;
  node->next = NULL;
  right->count = moved;
  left->count -= moved;
  right->owner = left->owner;
  right->next = left->next;
  right->prev = left;
  if (right->next)
    right->next->prev = right;
  left->next = right;
}

int
mulib_evtsched_cancel (mulib_evtsched_t *s, mulib_evtsched_notice_t *e)
{
  mulib_evtsched_bucket_t *b;
  if (!s || !e || !e->bucket || e->bucket->owner != s)
    {
      errno = EINVAL;
      return -1;
    }
  b = e->bucket;
  if (e->prev)
    e->prev->next = e->next;
  else
    b->first = e->next;
  if (e->next)
    e->next->prev = e->prev;
  else
    b->last = e->prev;
  e->prev = e->next = NULL;
  e->bucket = NULL;
  if (--b->count == 0 && b->prev)
    {
      b->prev->next = b->next;
      if (b->next)
        b->next->prev = b->prev;
      free (b);
    }
  return 0;
}

int
mulib_evtsched_schedule (mulib_evtsched_t *s, mulib_evtsched_notice_t *e,
                         int64_t deadline)
{
  mulib_evtsched_bucket_t *b;
  mulib_evtsched_notice_t *after;
  if (!s || !e)
    {
      errno = EINVAL;
      return -1;
    }
  if (e->bucket && mulib_evtsched_cancel (s, e) < 0)
    return -1;
  b = &s->first;
  b->owner = s;
  while (b->next && b->next->first->deadline <= deadline)
    b = b->next;
  after = b->last;
  while (after && after->deadline > deadline)
    after = after->prev;
  e->deadline = deadline;
  e->bucket = b;
  e->prev = after;
  e->next = after ? after->next : b->first;
  if (e->next)
    e->next->prev = e;
  else
    b->last = e;
  if (after)
    after->next = e;
  else
    b->first = e;
  ++b->count;
  mulib_evtsched_split (b);
  return 0;
}

int
mulib_evtsched_hold (mulib_evtsched_t *s, mulib_evtsched_notice_t *e,
                     int64_t delay)
{
  if (!s || !e || !e->bucket || e->bucket->owner != s || delay < 0)
    {
      errno = EINVAL;
      return -1;
    }
  if (e->deadline > INT64_MAX - delay)
    {
      errno = EOVERFLOW;
      return -1;
    }
  if (delay == 0)
    return 0;
  return mulib_evtsched_schedule (s, e, e->deadline + delay);
}

mulib_evtsched_notice_t *
mulib_evtsched_peek (const mulib_evtsched_t *s)
{
  if (!s)
    return NULL;
  if (s->first.first)
    return s->first.first;
  return s->first.next ? s->first.next->first : NULL;
}

mulib_evtsched_notice_t *
mulib_evtsched_pop (mulib_evtsched_t *s)
{
  mulib_evtsched_notice_t *e = mulib_evtsched_peek (s);
  if (e)
    mulib_evtsched_cancel (s, e);
  return e;
}

void
mulib_evtsched_clear (mulib_evtsched_t *s)
{
  while (mulib_evtsched_pop (s))
    {
    }
}

int
mulib_evtsched_dispatch (mulib_evtsched_t *s, int64_t now,
                         mulib_evtsched_fn run, void *ctx)
{
  if (!s || !run)
    {
      errno = EINVAL;
      return -1;
    }
  mulib_evtsched_notice_t *e = mulib_evtsched_peek (s);
  if (!e || e->deadline > now)
    return 0;
  mulib_evtsched_cancel (s, e);
  run (e, now, ctx);
  return 1;
}

int
mulib_evtsched_wait_one (mulib_evtsched_t *s, mulib_evtsched_fn run, void *ctx)
{
  if (!s || !run)
    {
      errno = EINVAL;
      return -1;
    }
  mulib_evtsched_notice_t *e = mulib_evtsched_peek (s);
  if (!e)
    return 0;
#ifdef __linux__
  for (;;)
    {
      struct itimerspec timer = { { 0, 0 }, { 0, 0 } };
      struct timespec now;
      uint64_t expirations;
      ssize_t n;
      int fd, err;
      if (timespec_get (&now, TIME_UTC) != TIME_UTC)
        {
          errno = EIO;
          return -1;
        }
      if (e->deadline <= now.tv_sec)
        return mulib_evtsched_dispatch (s, (int64_t)now.tv_sec, run, ctx);
      if (e->deadline > INT64_MAX / 1000000000)
        {
          errno = EOVERFLOW;
          return -1;
        }
      timer.it_value.tv_sec = (time_t)e->deadline;
      if ((int64_t)timer.it_value.tv_sec != e->deadline)
        {
          errno = EOVERFLOW;
          return -1;
        }
      /* Linux CLOCK_REALTIME ABI is 0 even when strict C hides the name. */
      fd = timerfd_create (0, TFD_CLOEXEC);
      if (fd < 0)
        return -1;
      err = 0;
      if (timerfd_settime (fd, TFD_TIMER_ABSTIME, &timer, NULL) < 0)
        err = errno;
      else
        {
          do
            {
              n = read (fd, &expirations, sizeof expirations);
            }
          while (n < 0 && errno == EINTR);
          if (n < 0)
            err = errno;
          /* A zero-length read can signal a discontinuous backwards clock
           * step. Recheck the wall clock before deciding whether to dispatch.
           */
          else if (n != 0 && n != (ssize_t)sizeof expirations)
            err = EIO;
        }
      close (fd);
      if (err)
        {
          errno = err;
          return -1;
        }
    }
#else
  (void)ctx;
  errno = ENOSYS;
  return -1;
#endif
}
#endif
