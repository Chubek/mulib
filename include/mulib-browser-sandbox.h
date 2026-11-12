/** @file mulib-browser-sandbox.h
 * @brief Deterministic resource-budget accounting and abort flags for browser
 * tasks. C11 single-header module. Define MULIB_IMPL_BROWSER_SANDBOX in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_SANDBOX_H
#define MULIB_BROWSER_SANDBOX_H
#include "mulib-browser-limits.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_sandbox
  {
    mulib_browser_limits_t limits;
    size_t response_bytes, nodes, script_steps, tasks;
    int cancelled;
  } mulib_browser_sandbox_t;
  /** @brief See manual/mulib-browser-sandbox.md for this operation's contract.
   */
  int mulib_browser_sandbox_init (mulib_browser_sandbox_t *s,
                                  const mulib_browser_limits_t *limits);
  /** @brief See manual/mulib-browser-sandbox.md for this operation's contract.
   */
  int mulib_browser_sandbox_charge (mulib_browser_sandbox_t *s, size_t bytes,
                                    size_t nodes, size_t steps, size_t tasks);
  /** @brief See manual/mulib-browser-sandbox.md for this operation's contract.
   */
  void mulib_browser_sandbox_cancel (mulib_browser_sandbox_t *s);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_SANDBOX_H */
#ifdef MULIB_IMPL_BROWSER_SANDBOX
#ifndef MULIB_IMPL_BROWSER_SANDBOX_ONCE
#define MULIB_IMPL_BROWSER_SANDBOX_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_sandbox_init (mulib_browser_sandbox_t *s,
                            const mulib_browser_limits_t *l)
{
  if (!s || !mulib_browser_limits_valid (l))
    return 0;
  memset (s, 0, sizeof *s);
  s->limits = *l;
  return 1;
}
int
mulib_browser_sandbox_charge (mulib_browser_sandbox_t *s, size_t bytes,
                              size_t nodes, size_t steps, size_t tasks)
{
  if (!s || s->cancelled || !mulib_browser_limits_valid (&s->limits)
      || s->response_bytes > s->limits.max_response_bytes
      || s->nodes > s->limits.max_nodes
      || s->script_steps > s->limits.max_script_steps
      || s->tasks > s->limits.max_tasks
      || bytes > s->limits.max_response_bytes - s->response_bytes
      || nodes > s->limits.max_nodes - s->nodes
      || steps > s->limits.max_script_steps - s->script_steps
      || tasks > s->limits.max_tasks - s->tasks)
    return 0;
  s->response_bytes += bytes;
  s->nodes += nodes;
  s->script_steps += steps;
  s->tasks += tasks;
  return 1;
}
void
mulib_browser_sandbox_cancel (mulib_browser_sandbox_t *s)
{
  if (s)
    s->cancelled = 1;
}
#endif
#endif
