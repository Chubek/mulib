/** @file mulib-browser-engine.h
 * @brief Embeddable nonvisual static-HTML browser document and event engine.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_ENGINE in exactly one C
 * translation unit. Static HTML profile only; JavaScript execution, Fetch
 * conformance and CSS layout are NOT implemented by this header.
 */
#ifndef MULIB_BROWSER_ENGINE_H
#define MULIB_BROWSER_ENGINE_H
#include "mulib-browser-documentload.h"
#include "mulib-browser-fetch.h"
#include "mulib-browser-limits.h"
#include "mulib-browser-navigation.h"
#include "mulib-browser-stream.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_engine
  {
    mulib_browser_fetch_t fetch;
    mulib_browser_navigation_t history;
    mulib_browser_stream_t stream;
    mulib_html_tree_node_t *document;
    mulib_browser_limits_t limits;
    int navigating;
  } mulib_browser_engine_t;
  /** @brief See manual/mulib-browser-engine.md for this operation's contract.
   */
  int mulib_browser_engine_init (mulib_browser_engine_t *engine,
                                 const mulib_browser_fetch_t *fetch,
                                 const mulib_browser_limits_t *limits);
  /** @brief See manual/mulib-browser-engine.md for this operation's contract.
   */
  int mulib_browser_engine_navigate (mulib_browser_engine_t *engine,
                                     const char *url, unsigned *status);
  /** @brief See manual/mulib-browser-engine.md for this operation's contract.
   */
  void mulib_browser_engine_destroy (mulib_browser_engine_t *engine);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_ENGINE_H */
#ifdef MULIB_IMPL_BROWSER_ENGINE
#ifndef MULIB_IMPL_BROWSER_ENGINE_ONCE
#define MULIB_IMPL_BROWSER_ENGINE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_engine_init (mulib_browser_engine_t *e,
                           const mulib_browser_fetch_t *f,
                           const mulib_browser_limits_t *l)
{
  if (!e || !f || !f->client.transport || !mulib_browser_limits_valid (l))
    return 0;
  mulib_browser_fetch_t fetch = *f;
  mulib_browser_limits_t limits = *l;
  memset (e, 0, sizeof *e);
  e->fetch = fetch;
  e->limits = limits;
  e->history.limit = limits.max_urls;
  return 1;
}
int
mulib_browser_engine_navigate (mulib_browser_engine_t *e, const char *url,
                               unsigned *status)
{
  if (!e || e->navigating || !url || !status
      || !mulib_browser_limits_valid (&e->limits) || strlen (url) > 8192
      || e->stream.sequence == UINT64_MAX)
    return 0;
  char *owned = mulib_shylock_dup (url, strlen (url));
  if (!owned)
    return 0;
  e->navigating = 1;
  mulib_html_tree_node_t *root = NULL;
  unsigned code;
  int ok = mulib_browser_documentload_bounded (
      &e->fetch, owned, e->limits.max_nodes, e->limits.max_depth,
      e->limits.max_response_bytes, &root, &code);
  if (ok)
    ok = mulib_browser_navigation_visit (&e->history, owned);
  if (ok)
    {
      mulib_html_tree_free (e->document);
      e->document = root;
      *status = code;
      (void)mulib_browser_stream_emit (&e->stream,
                                       MULIB_BROWSER_CHANNEL_NETWORK, code, 0,
                                       owned, strlen (owned));
    }
  else
    mulib_html_tree_free (root);
  e->navigating = 0;
  free (owned);
  return ok;
}
void
mulib_browser_engine_destroy (mulib_browser_engine_t *e)
{
  if (e && !e->navigating && !e->stream.dispatching)
    {
      mulib_html_tree_free (e->document);
      mulib_browser_navigation_free (&e->history);
      mulib_browser_stream_free (&e->stream);
      memset (e, 0, sizeof *e);
    }
}
#endif
#endif
