# browser-engine

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Embeddable nonvisual static-HTML browser document and event engine.

## Contract and supported profile

Init requires fresh/released storage, a transport and validated limits; it
copies fetch configuration and sets the history count budget. Navigate fetches
static text/html and enforces response bytes, nodes and configured tree depth,
then commits document/history/status together. Failures preserve the old document
and history. `mulib_browser_engine_traverse` reloads the entry at a signed
offset from the current entry (zero reloads it), committing the document,
cursor and status together without appending or pruning history. Unlike
moving the standalone history cursor, traversal uses the engine's transport
and document budgets; this is a reload, not back/forward cache restoration.
An out-of-range offset does not invoke transport. After commit either operation
emits a synchronous network event: observer rejection does not turn success
into failure or undo the document. Navigation and traversal are rejected
during navigation/dispatch; destroy during navigation/dispatch is ignored. Destroy
otherwise frees document/history/stream. Script/task budgets are not exercised
because this engine executes no scripts/tasks; no CSS rendering, CORS, subresource
loading, cookies or cache integration is implied.

## Integration and ownership

Include `kernels/mulib-browser-engine.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_DOCUMENTLOAD`, `MULIB_IMPL_BROWSER_ENGINE`, `MULIB_IMPL_BROWSER_FETCH`, `MULIB_IMPL_BROWSER_LIMITS`, `MULIB_IMPL_BROWSER_NAVIGATION`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_BROWSER_STREAM`, `MULIB_IMPL_FETCH`, `MULIB_IMPL_FETCH_POLICY`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_BOUNDED_TEXT`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-engine.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c) and the
[offline timer/turn traversal regression](../tests/test-browser-engine-timed-traversal.c),
with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

## Browser host integration status

| Ledger area | Verified slice | Not integrated |
| --- | --- | --- |
| Engine, documentload, navigation | Reload an existing static HTML history entry and commit document, cursor and status together. Preserve all three on failure. | Browsing context ownership, same-document navigation, joint session history and BFCache. |
| Eventloop, webtimers | A host can poll a timer, post a navigation task, and run observer-posted microtasks before the next task. | Internal timer dispatch, asynchronous I/O and browser scheduling phases. |
| Context, window and document lifecycle | Outside this slice; history contains URLs, not live documents. | Context/window identity, readiness/load blockers, unload and document discard. |
| Script and resource orchestration | No scripts run, and no subresources are fetched. | Script preparation/queues, module loading and subresource scheduling. |
| Frame, viewport, observers and input | Not used by the static engine. | Frame scheduling, geometry, trusted input and activation. |
| Profiles, downloads, protocol handlers and process providers | Not used by the static engine. | Host provider integration and associated authorization. |

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`kernels/mulib-browser-engine.h`](../kernels/mulib-browser-engine.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
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
  /** @brief Reload a relative history entry without changing history length.
   * Zero reloads the current entry. On failure, document, cursor and status
   * are unchanged. See manual/mulib-browser-engine.md. */
  int mulib_browser_engine_traverse (mulib_browser_engine_t *engine,
                                     int delta, unsigned *status);
  /** @brief See manual/mulib-browser-engine.md for this operation's contract.
   */
  void mulib_browser_engine_destroy (mulib_browser_engine_t *engine);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_ENGINE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-browser-engine.h`](../kernels/mulib-browser-engine.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_ENGINE` |
| Repository-local includes | [mulib-browser-documentload](mulib-browser-documentload.md), [mulib-browser-fetch](mulib-browser-fetch.md), [mulib-browser-limits](mulib-browser-limits.md), [mulib-browser-navigation](mulib-browser-navigation.md), [mulib-browser-stream](mulib-browser-stream.md) |
| Complete example | [`examples/modules/browser-engine.c`](../examples/modules/browser-engine.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/browser-engine.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
