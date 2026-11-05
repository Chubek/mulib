/**
 * @file mulib-channel.h
 * @brief CSP buffered/rendezvous channels, directional views and atomic select.
 * Define MULIB_IMPL_CHANNEL and dependency switches independently.
 */
#ifndef MULIB_CHANNEL_H
#define MULIB_CHANNEL_H
#include "mulib-fiber.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_channel mulib_channel_t;
typedef struct mulib_channel_sender { mulib_channel_t *channel; } mulib_channel_sender_t;
typedef struct mulib_channel_receiver { mulib_channel_t *channel; } mulib_channel_receiver_t;
enum {
  MULIB_CHANNEL_OK = 0, MULIB_CHANNEL_INVALID = -1,
  MULIB_CHANNEL_NOMEM = -2, MULIB_CHANNEL_SYSTEM = -3,
  MULIB_CHANNEL_CLOSED = -4, MULIB_CHANNEL_TIMEOUT = -5,
  MULIB_CHANNEL_DEFAULT = -6, MULIB_CHANNEL_BUSY = -7
};
enum { MULIB_CHANNEL_SEND = 1, MULIB_CHANNEL_RECEIVE = 2 };
typedef struct mulib_channel_case {
  mulib_channel_t *channel; /**< NULL disables this case, like a Go nil channel. */
  int operation;
  const void *send_value;
  void *receive_value;
} mulib_channel_case_t;
/** Allocate a fixed-element byte-copy channel; capacity 0 is a rendezvous.
 * No ownership of pointers inside copied values is transferred automatically. */
int mulib_channel_create (mulib_channel_t **out, size_t element_size, size_t capacity);
/** Free after all operations and borrowed views are finished. BUSY if waiters
 * remain. The caller must exclude concurrent entry into any channel operation. */
int mulib_channel_destroy (mulib_channel_t *);
/** Close once. Buffered values drain before receives return CLOSED with zeroed
 * output. Pending/future sends return CLOSED. Repeated close returns CLOSED. */
int mulib_channel_close (mulib_channel_t *);
/** Borrow a send-only view; its lifetime is bounded by the channel's lifetime. */
mulib_channel_sender_t mulib_channel_sender (mulib_channel_t *);
/** Borrow a receive-only view. */
mulib_channel_receiver_t mulib_channel_receiver (mulib_channel_t *);
/** Send a copied value. Timeout is relative milliseconds; 0 tries once,
 * MULIB_XWAIT_FOREVER blocks. Fiber callers park without blocking their OS thread. */
int mulib_channel_send (mulib_channel_sender_t, const void *, uint64_t timeout_ms);
/** Receive a value. Output is unchanged on timeout, zeroed on drained close. */
int mulib_channel_receive (mulib_channel_receiver_t, void *, uint64_t timeout_ms);
/** Construct a send select case from a directional view. */
mulib_channel_case_t mulib_channel_send_case (mulib_channel_sender_t, const void *);
/** Construct a receive select case from a directional view. */
mulib_channel_case_t mulib_channel_receive_case (mulib_channel_receiver_t, void *);
/** Commit exactly one ready operation, returning its status and index. Ready
 * cases use rotating priority. If none is ready, has_default returns DEFAULT;
 * otherwise wait up to timeout_ms. No selection sets *selected = SIZE_MAX.
 * Empty/all-nil selections can default, time out, or block forever. */
int mulib_channel_select (const mulib_channel_case_t *cases, size_t count,
                          int has_default, uint64_t timeout_ms, size_t *selected);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CHANNEL) && !defined(MULIB_CHANNEL_IMPLEMENTED)
#define MULIB_CHANNEL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
typedef struct mulib_channel_selection_ mulib_channel_selection_;
typedef struct mulib_channel_waiter_ {
  struct mulib_channel_waiter_ *next;
  mulib_channel_selection_ *selection;
  const mulib_channel_case_t *item;
  size_t index;
} mulib_channel_waiter_;
struct mulib_channel_selection_ {
  mulib_fiber_t *fiber;
  mulib_xcond_t condition;
  int done, status;
  size_t selected;
};
struct mulib_channel {
  unsigned char *buffer;
  size_t size, capacity, head, count;
  int closed;
  mulib_channel_waiter_ *first, *last;
};
/* One lock linearizes multi-channel selections, including rendezvous between
 * two selectors. No callback/context switch occurs while holding this lock. */
static mulib_xmutex_t mulib_channel_mutex_ = MULIB_XMUTEX_INIT;
static size_t mulib_channel_rotation_;
static void mulib_channel_complete_ (mulib_channel_waiter_ *w, int status)
{
  mulib_channel_selection_ *s = w->selection;
  s->done = 1; s->selected = w->index; s->status = status;
  if (s->fiber) mulib_fiber_unpark (s->fiber);
  else (void)mulib_xcond_signal (&s->condition);
}
static mulib_channel_waiter_ *mulib_channel_partner_ (mulib_channel_t *c, int op)
{
  mulib_channel_waiter_ *w;
  for (w = c->first; w; w = w->next)
    if (!w->selection->done && w->item->operation == op) return w;
  return NULL;
}
static size_t mulib_channel_tail_ (mulib_channel_t *c)
{ return c->count >= c->capacity - c->head ? c->count - (c->capacity - c->head) : c->head + c->count; }
static int mulib_channel_attempt_ (const mulib_channel_case_t *item)
{
  mulib_channel_t *c = item->channel;
  mulib_channel_waiter_ *w;
  if (!c) return MULIB_CHANNEL_BUSY;
  if (item->operation == MULIB_CHANNEL_SEND) {
    if (c->closed) return MULIB_CHANNEL_CLOSED;
    w = mulib_channel_partner_ (c, MULIB_CHANNEL_RECEIVE);
    if (w) {
      memcpy (w->item->receive_value, item->send_value, c->size);
      mulib_channel_complete_ (w, 0); return 0;
    }
    if (c->count < c->capacity) {
      memcpy (c->buffer + mulib_channel_tail_ (c) * c->size, item->send_value, c->size);
      ++c->count; return 0;
    }
  } else {
    if (c->count) {
      memcpy (item->receive_value, c->buffer + c->head * c->size, c->size);
      c->head = (c->head + 1) % c->capacity; --c->count;
      /* Refill from the oldest waiting sender before another sender can barge. */
      w = c->closed ? NULL : mulib_channel_partner_ (c, MULIB_CHANNEL_SEND);
      if (w) {
        memcpy (c->buffer + mulib_channel_tail_ (c) * c->size, w->item->send_value, c->size);
        ++c->count; mulib_channel_complete_ (w, 0);
      }
      return 0;
    }
    if (c->closed) {
      memset (item->receive_value, 0, c->size); return MULIB_CHANNEL_CLOSED;
    }
    w = mulib_channel_partner_ (c, MULIB_CHANNEL_SEND);
    if (w) {
      memcpy (item->receive_value, w->item->send_value, c->size);
      mulib_channel_complete_ (w, 0); return 0;
    }
  }
  return MULIB_CHANNEL_BUSY;
}
int mulib_channel_create (mulib_channel_t **out, size_t size, size_t capacity)
{
  mulib_channel_t *c;
  if (!out) return MULIB_CHANNEL_INVALID;
  *out = NULL;
  if (!size || capacity > SIZE_MAX / size) return MULIB_CHANNEL_INVALID;
  c = (mulib_channel_t *)calloc (1, sizeof (*c));
  if (!c) return MULIB_CHANNEL_NOMEM;
  if (capacity) {
    c->buffer = (unsigned char *)malloc (size * capacity);
    if (!c->buffer) { free (c); return MULIB_CHANNEL_NOMEM; }
  }
  c->size = size; c->capacity = capacity; *out = c; return 0;
}
int mulib_channel_destroy (mulib_channel_t *c)
{
  if (!c) return 0;
  (void)mulib_xmutex_lock (&mulib_channel_mutex_);
  if (c->first) {
    (void)mulib_xmutex_unlock (&mulib_channel_mutex_); return MULIB_CHANNEL_BUSY;
  }
  (void)mulib_xmutex_unlock (&mulib_channel_mutex_);
  free (c->buffer); free (c); return 0;
}
int mulib_channel_close (mulib_channel_t *c)
{
  mulib_channel_waiter_ *w;
  if (!c) return MULIB_CHANNEL_INVALID;
  (void)mulib_xmutex_lock (&mulib_channel_mutex_);
  if (c->closed) {
    (void)mulib_xmutex_unlock (&mulib_channel_mutex_); return MULIB_CHANNEL_CLOSED;
  }
  c->closed = 1;
  for (w = c->first; w; w = w->next) {
    if (w->selection->done) continue;
    if (w->item->operation == MULIB_CHANNEL_RECEIVE) {
      /* A queued receiver implies an empty buffer. */
      memset (w->item->receive_value, 0, c->size);
    }
    mulib_channel_complete_ (w, MULIB_CHANNEL_CLOSED);
  }
  (void)mulib_xmutex_unlock (&mulib_channel_mutex_); return 0;
}
mulib_channel_sender_t mulib_channel_sender (mulib_channel_t *c)
{ mulib_channel_sender_t view; view.channel = c; return view; }
mulib_channel_receiver_t mulib_channel_receiver (mulib_channel_t *c)
{ mulib_channel_receiver_t view; view.channel = c; return view; }
mulib_channel_case_t mulib_channel_send_case (mulib_channel_sender_t s, const void *v)
{
  mulib_channel_case_t c;
  c.channel = s.channel; c.operation = MULIB_CHANNEL_SEND;
  c.send_value = v; c.receive_value = NULL; return c;
}
mulib_channel_case_t mulib_channel_receive_case (mulib_channel_receiver_t r, void *v)
{
  mulib_channel_case_t c;
  c.channel = r.channel; c.operation = MULIB_CHANNEL_RECEIVE;
  c.receive_value = v; c.send_value = NULL; return c;
}
int mulib_channel_select (const mulib_channel_case_t *cases, size_t count,
                         int has_default, uint64_t timeout, size_t *selected)
{
  mulib_channel_selection_ s;
  mulib_channel_waiter_ *nodes = NULL;
  size_t i, start;
  uint64_t deadline = mulib_xdeadline (timeout);
  int rc = MULIB_CHANNEL_TIMEOUT;
  if (!selected) return MULIB_CHANNEL_INVALID;
  *selected = SIZE_MAX;
  if ((count && !cases) || count > SIZE_MAX / sizeof (*nodes)) return MULIB_CHANNEL_INVALID;
  for (i = 0; i < count; ++i) {
    if (cases[i].operation != MULIB_CHANNEL_SEND && cases[i].operation != MULIB_CHANNEL_RECEIVE)
      return MULIB_CHANNEL_INVALID;
    if (cases[i].channel && (cases[i].operation == MULIB_CHANNEL_SEND
         ? !cases[i].send_value : !cases[i].receive_value)) return MULIB_CHANNEL_INVALID;
  }
  memset (&s, 0, sizeof (s)); s.fiber = mulib_fiber_current ();
  if (!s.fiber && mulib_xcond_init (&s.condition)) return MULIB_CHANNEL_SYSTEM;
  (void)mulib_xmutex_lock (&mulib_channel_mutex_);
  start = count ? mulib_channel_rotation_++ % count : 0;
  i = start;
  if (count) do {
    rc = mulib_channel_attempt_ (&cases[i]);
    if (rc != MULIB_CHANNEL_BUSY) { *selected = i; goto done; }
    i = i + 1 == count ? 0 : i + 1;
  } while (i != start);
  if (has_default) { rc = MULIB_CHANNEL_DEFAULT; goto done; }
  if (!timeout) { rc = MULIB_CHANNEL_TIMEOUT; goto done; }
  if (count) {
    nodes = (mulib_channel_waiter_ *)calloc (count, sizeof (*nodes));
    if (!nodes) { rc = MULIB_CHANNEL_NOMEM; goto done; }
  }
  /* Register only after probing every case: a select cannot rendezvous with itself. */
  for (i = 0; i < count; ++i) {
    mulib_channel_t *c = cases[i].channel;
    if (!c) continue;
    nodes[i].selection = &s; nodes[i].item = &cases[i]; nodes[i].index = i;
    if (c->last) c->last->next = &nodes[i]; else c->first = &nodes[i];
    c->last = &nodes[i];
  }
  while (!s.done) {
    if (deadline != UINT64_MAX && mulib_xclock_ms () >= deadline) break;
    if (s.fiber) {
      (void)mulib_xmutex_unlock (&mulib_channel_mutex_);
      (void)mulib_fiber_park_until (deadline);
      (void)mulib_xmutex_lock (&mulib_channel_mutex_);
    } else {
      rc = mulib_xcond_wait_until (&s.condition, &mulib_channel_mutex_, deadline);
      if (rc && rc != MULIB_XTHREAD_TIMEOUT) break;
    }
  }
  rc = s.done ? s.status : rc == MULIB_XTHREAD_SYSTEM ? MULIB_CHANNEL_SYSTEM : MULIB_CHANNEL_TIMEOUT;
  if (s.done) *selected = s.selected;
  /* Remove every losing case before the stack-local selection disappears. */
  for (i = 0; i < count; ++i) {
    mulib_channel_t *c = cases[i].channel;
    mulib_channel_waiter_ **link, *previous = NULL;
    if (!c) continue;
    for (link = &c->first; *link != &nodes[i]; link = &(*link)->next) previous = *link;
    *link = nodes[i].next;
    if (c->last == &nodes[i]) c->last = previous;
  }
  if (s.fiber) mulib_fiber_clear_wake ();
done:
  (void)mulib_xmutex_unlock (&mulib_channel_mutex_);
  if (!s.fiber) mulib_xcond_destroy (&s.condition);
  free (nodes); return rc;
}
int mulib_channel_send (mulib_channel_sender_t s, const void *v, uint64_t ms)
{
  mulib_channel_case_t c = mulib_channel_send_case (s, v); size_t selected;
  return mulib_channel_select (&c, 1, 0, ms, &selected);
}
int mulib_channel_receive (mulib_channel_receiver_t r, void *v, uint64_t ms)
{
  mulib_channel_case_t c = mulib_channel_receive_case (r, v); size_t selected;
  return mulib_channel_select (&c, 1, 0, ms, &selected);
}
#endif
