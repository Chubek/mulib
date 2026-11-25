#define MULIB_IMPL_EVTSCHED
#include "mulib-evtsched.h"
#include <assert.h>

/* Mixed operations compared with an independent stable minimum search. */
static void
ordering (void)
{
  enum
  {
    N = 107
  };
  mulib_evtsched_t s = { 0 };
  mulib_evtsched_notice_t events[N] = { { 0 } };
  unsigned order[N] = { 0 };
  int64_t due[N] = { 0 };
  uint32_t state = 37;
  for (unsigned step = 1; step <= 8000; ++step)
    {
      state = state * UINT32_C (1664525) + UINT32_C (1013904223);
      size_t i = step <= N ? step - 1 : (state >> 8) % N;
      size_t best = N;
      for (size_t j = 0; j < N; ++j)
        if (order[j]
            && (best == N || due[j] < due[best]
                || (due[j] == due[best] && order[j] < order[best])))
          best = j;
      assert (mulib_evtsched_peek (&s) == (best == N ? NULL : &events[best]));
      if (step > N && state % 7 == 0 && best < N)
        {
          assert (mulib_evtsched_pop (&s) == &events[best]);
          order[best] = 0;
        }
      else if (step > N && order[i] && state % 5 == 0)
        {
          assert (mulib_evtsched_cancel (&s, &events[i]) == 0);
          order[i] = 0;
        }
      else if (step > N && order[i] && state % 3 == 0)
        {
          assert (mulib_evtsched_hold (&s, &events[i], 2) == 0);
          due[i] += 2;
          order[i] = step;
        }
      else
        {
          due[i] = step <= N ? 1 : (int64_t)(state % 11) - 5;
          assert (mulib_evtsched_schedule (&s, &events[i], due[i]) == 0);
          order[i] = step;
        }
    }
  mulib_evtsched_clear (&s);
  for (size_t i = 0; i < N; ++i)
    assert (!events[i].bucket);
}

static void
recur (mulib_evtsched_notice_t *e, int64_t now, void *context)
{
  unsigned *calls = (unsigned *)e->data;
  assert (!e->bucket && now >= e->deadline);
  if (++*calls == 1)
    assert (mulib_evtsched_schedule ((mulib_evtsched_t *)context, e, now + 7)
            == 0);
}

int
main (void)
{
  mulib_evtsched_t s = { 0 }, other = { 0 };
  mulib_evtsched_notice_t e = { 0 }, peer = { 0 };
  unsigned calls = 0;
  e.data = &calls;
  ordering ();
  assert (mulib_evtsched_wait_one (&s, recur, &s) == 0);
  assert (mulib_evtsched_cancel (&s, &e) == -1 && errno == EINVAL);
  assert (mulib_evtsched_schedule (&s, &e, INT64_MAX) == 0);
  assert (mulib_evtsched_schedule (&other, &e, 0) == -1 && errno == EINVAL);
  assert (mulib_evtsched_hold (&s, &e, 1) == -1 && errno == EOVERFLOW);
#ifdef __linux__
  assert (mulib_evtsched_wait_one (&s, recur, &s) == -1 && errno == EOVERFLOW);
#endif
  assert (e.deadline == INT64_MAX && mulib_evtsched_peek (&s) == &e);
  assert (mulib_evtsched_schedule (&s, &e, INT64_MIN) == 0);
  assert (mulib_evtsched_schedule (&s, &peer, INT64_MIN) == 0);
  assert (mulib_evtsched_hold (&s, &e, -1) == -1 && errno == EINVAL);
  assert (mulib_evtsched_hold (&s, &e, 0) == 0);
  assert (mulib_evtsched_dispatch (&s, 2, recur, &s) == 1 && calls == 1);
  assert (mulib_evtsched_pop (&s) == &peer);
  assert (mulib_evtsched_dispatch (&s, 8, recur, &s) == 0);
  assert (mulib_evtsched_dispatch (&s, 9, recur, &s) == 1 && calls == 2);
#ifdef __linux__
  assert (mulib_evtsched_schedule (&s, &e, (int64_t)time (NULL) + 1) == 0);
  assert (mulib_evtsched_wait_one (&s, recur, &s) == 1 && calls == 3);
#endif
  assert (!mulib_evtsched_peek (&s));
  mulib_evtsched_clear (&s);
  return 0;
}
