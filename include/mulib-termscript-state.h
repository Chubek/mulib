/** @file mulib-termscript-state.h
 * @brief Deterministic finite-state transitions for Termscript/native hosts.
 * Define MULIB_IMPL_TERMSCRIPT_STATE once; no implementation dependencies.
 */
#ifndef MULIB_TERMSCRIPT_STATE_H
#define MULIB_TERMSCRIPT_STATE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int64_t source, event, target;
} mulib_termscript_transition_t;

typedef struct {
  /* Caller storage; treat all fields as read-only after initialization. */
  const mulib_termscript_transition_t *transitions;
  size_t count;
  int64_t state;
  unsigned active;
} mulib_termscript_state_t;

typedef int (*mulib_termscript_transition_fn)(void *, const mulib_termscript_transition_t *);

/* The immutable table is borrowed. It must be sorted by (source, event) in
 * ascending signed order, without duplicates. Initial state may be a sink.
 * An empty table is valid. The output object is unchanged on invalid input. */
int mulib_termscript_state_init(mulib_termscript_state_t *, int64_t initial,
                                const mulib_termscript_transition_t *, size_t count);

/* Find a transition in logarithmic time; copy it to out if found. */
int mulib_termscript_state_find(const mulib_termscript_state_t *, int64_t event,
                                mulib_termscript_transition_t *out);

/* 1 committed, 0 no transition or action rejected, -1 invalid/reentrant.
 * A NULL action commits unconditionally; a non-NULL action must return 1.
 * The callback observes the old state and must not modify the machine.
 * External application effects performed by the callback are not rolled back. */
int mulib_termscript_state_send(mulib_termscript_state_t *, int64_t event,
                                mulib_termscript_transition_fn, void *context);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT_STATE) && !defined(MULIB_TERMSCRIPT_STATE_IMPLEMENTED)
#define MULIB_TERMSCRIPT_STATE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif

static int
mulib_termscript_state_less_(const mulib_termscript_transition_t *t, int64_t source, int64_t event)
{
  return t->source < source || (t->source == source && t->event < event);
}

int
mulib_termscript_state_init(mulib_termscript_state_t *s, int64_t initial,
                            const mulib_termscript_transition_t *table, size_t n)
{
  if (!s || (!table && n) || n > SIZE_MAX / sizeof *table) return 0;
  for (size_t i = 1; i < n; ++i)
    if (!mulib_termscript_state_less_(table + i - 1, table[i].source, table[i].event))
      return 0;
  s->transitions = table;
  s->count = n;
  s->state = initial;
  s->active = 0;
  return 1;
}

int
mulib_termscript_state_find(const mulib_termscript_state_t *s, int64_t event,
                            mulib_termscript_transition_t *out)
{
  if (!s || !out || !s->transitions) return 0;
  size_t lo = 0, hi = s->count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (mulib_termscript_state_less_(s->transitions + mid, s->state, event)) lo = mid + 1;
    else hi = mid;
  }
  if (lo == s->count || s->transitions[lo].source != s->state
      || s->transitions[lo].event != event) return 0;
  *out = s->transitions[lo];
  return 1;
}

int
mulib_termscript_state_send(mulib_termscript_state_t *s, int64_t event,
                            mulib_termscript_transition_fn action, void *user)
{
  mulib_termscript_transition_t t;
  if (!s || s->active) return -1;
  if (!mulib_termscript_state_find(s, event, &t)) return 0;
  s->active = 1;
  int accepted = !action || action(user, &t) == 1;
  s->active = 0;
  if (accepted) s->state = t.target;
  return accepted;
}

#ifdef __cplusplus
}
#endif
#endif
