/** @file mulib-browser-limits.h
 * @brief Validated resource limits for untrusted document execution.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_LIMITS in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_LIMITS_H
#define MULIB_BROWSER_LIMITS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_limits
  {
    size_t max_response_bytes, max_nodes, max_depth, max_tasks,
        max_script_steps, max_urls;
  } mulib_browser_limits_t;
  /** @brief See manual/mulib-browser-limits.md for this operation's contract.
   */
  int mulib_browser_limits_default (mulib_browser_limits_t *out);
  /** @brief See manual/mulib-browser-limits.md for this operation's contract.
   */
  int mulib_browser_limits_valid (const mulib_browser_limits_t *limits);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_LIMITS_H */
#ifdef MULIB_IMPL_BROWSER_LIMITS
#ifndef MULIB_IMPL_BROWSER_LIMITS_ONCE
#define MULIB_IMPL_BROWSER_LIMITS_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_limits_default (mulib_browser_limits_t *o)
{
  if (!o)
    return 0;
  *o = (mulib_browser_limits_t){
    16u * 1024u * 1024u, 100000, 128, 10000, 1000000, 10000
  };
  return 1;
}
int
mulib_browser_limits_valid (const mulib_browser_limits_t *l)
{
  return l && l->max_response_bytes >= 1024
         && l->max_response_bytes <= 512u * 1024u * 1024u && l->max_nodes
         && l->max_nodes <= 10000000 && l->max_depth && l->max_depth <= 1024
         && l->max_tasks && l->max_tasks <= 1000000 && l->max_script_steps
         && l->max_urls;
}
#endif
#endif
