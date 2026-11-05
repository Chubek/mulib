/** @file mulib-evtlistener.h
 * @brief Callback listeners for event streams and parser-produced events.
 * Define MULIB_IMPL_EVTLISTENER and MULIB_IMPL_EVTSTREAM once. No allocation
 * or AST implementation is required; event nodes and callback state are borrowed.
 */
#ifndef MULIB_EVTLISTENER_H
#define MULIB_EVTLISTENER_H
#include "mulib-evtstream.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Return positive to continue, zero to stop, negative on error.
   * Events and their nodes are borrowed only for the duration of the call. */
  typedef int (*mulib_evtlistener_event_fn) (const mulib_event_t *, void *);
  /** A non-owning listener. Initialize with { callback, user }; a NULL user
   * is valid. No events or nodes are retained, and no tree is required. */
  typedef struct mulib_evtlistener
  {
    mulib_evtlistener_event_fn event;
    void *user;
  } mulib_evtlistener_t;
  /** Deliver one event synchronously; 1 continue, 0 stopped, -1 invalid/error.
   * A listener callback, event node and ENTER/LEAVE kind are required. */
  int mulib_evtlistener_push (const mulib_evtlistener_t *, const mulib_event_t *);
  /** Drain a borrowed pull stream; 1 exhausted, 0 stopped, -1 invalid/error.
   * The delivered event is already consumed on stop/error. May be resumed.
   * The callback must not mutate the stream or its tree during delivery. */
  int mulib_evtlistener_listen (mulib_evtstream_t *, const mulib_evtlistener_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EVTLISTENER) && !defined(MULIB_EVTLISTENER_IMPLEMENTED)
#define MULIB_EVTLISTENER_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_evtlistener_push (const mulib_evtlistener_t *l, const mulib_event_t *e)
  {
    int r;
    if (!l || !l->event || !e || !e->node
        || (e->kind != MULIB_EVENT_ENTER && e->kind != MULIB_EVENT_LEAVE))
      return -1;
    r = l->event (e, l->user);
    return r < 0 ? -1 : r ? 1 : 0;
  }
  int
  mulib_evtlistener_listen (mulib_evtstream_t *s, const mulib_evtlistener_t *l)
  {
    mulib_event_t e;
    int r;
    if (!s || !l || !l->event) return -1;
    while ((r = mulib_evtstream_next (s, &e)) == 1)
      {
        r = mulib_evtlistener_push (l, &e);
        if (r != 1) return r;
      }
    return r < 0 ? -1 : 1;
  }
#ifdef __cplusplus
}
#endif
#endif
