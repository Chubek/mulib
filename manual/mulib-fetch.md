# fetch

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Transport-agnostic bounded HTTP retrieval with explicit policy enforcement.

## Contract and supported profile

A client borrows its transport function/context. Get checks policy before
calling the synchronous transport. The callback must return 1 for a successful
retrieval and report the final status (200..599, including HTTP errors). It
must handle interim 1xx responses internally and cannot hand off an upgrade.
Statuses 204, 205 and 304 must have no body bytes.

The callback must use the supplied owned temporary buffer, respect its byte
limit and return a NUL-terminated Content-Type value (empty if absent). Control
bytes other than horizontal tab, including DEL, are rejected; this is not a MIME
syntax parser. The response limit defaults to 16 MiB and is rechecked
independently of mutable buffer metadata. Nonempty successful bodies append to
output, subject to the caller buffer's own limit. Empty successful bodies leave
that buffer untouched. Status and type publish only after success. Failure
preserves caller outputs and releases partial transport output.

The callback must maintain valid buffer ownership and allocation metadata and
enforce limits while receiving or decoding. It must also enforce TLS,
resolved-peer policy and every redirect; this layer does no I/O and cannot
make an untrusted callback safe.

## Integration and ownership

Include `kernels/mulib-fetch.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_FETCH`, `MULIB_IMPL_FETCH_POLICY`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_BOUNDED_TEXT`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/fetch.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).
[`tests/test_browser_network.c`](../tests/test_browser_network.c) checks the
transport handoff and output preservation through the browser adapter.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`kernels/mulib-fetch.h`](../kernels/mulib-fetch.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-fetch.h
 * @brief Transport-agnostic bounded HTTP retrieval with explicit policy
 * enforcement. C11 single-header module. Define MULIB_IMPL_FETCH in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_FETCH_H
#define MULIB_FETCH_H
#include "mulib-fetch-policy.h"
#include "mulib-bounded-text.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_fetch_transport_fn) (void *ctx, const char *url,
                                           mulib_bounded_text_buffer_t *body,
                                           unsigned *status,
                                           char *content_type, size_t cap);
  typedef struct mulib_fetch_client
  {
    mulib_fetch_policy_t policy;
    mulib_fetch_transport_fn transport;
    void *ctx;
  } mulib_fetch_client_t;
  /** @brief See manual/mulib-fetch.md for this operation's contract. */
  int mulib_fetch_get (const mulib_fetch_client_t *client, const char *url,
                       mulib_bounded_text_buffer_t *body, unsigned *status,
                       char *content_type, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FETCH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-fetch.h`](../kernels/mulib-fetch.h) |
| Implementation switch | `MULIB_IMPL_FETCH` |
| Repository-local includes | [mulib-bounded-text](mulib-bounded-text.md), [mulib-fetch-policy](mulib-fetch-policy.md) |
| Complete example | [`examples/modules/fetch.c`](../examples/modules/fetch.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/fetch.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
