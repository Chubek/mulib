# websearch-provider

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Registered search providers with encoded query-URL generation.

## Contract and supported profile

The endpoint record is `mulib_websearch_provider_config_t`; the existing
`mulib_websearch_provider_t` enum in `mulib-websearch.h` remains unchanged.

Initialization copies a nonempty name and validated HTTPS endpoint without
a fragment, plus a positive result-limit metadata field. URL generation appends
q and start parameters, using ASCII form-query encoding (space becomes +) and
offset<=100000; query is at most 4096 bytes. Existing queries are preserved.
max_results is metadata, not a provider-specific URL parameter. The entire URL
is staged; failed validation/allocation preserves output. No search is executed.

## Integration and ownership

Include `include/mulib-websearch-provider.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_WEBSEARCH_PROVIDER`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/websearch-provider.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-websearch-provider.h`](../include/mulib-websearch-provider.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-websearch-provider.h
 * @brief Registered search providers with encoded query-URL generation.
 * C11 single-header module. Define MULIB_IMPL_WEBSEARCH_PROVIDER in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_WEBSEARCH_PROVIDER_H
#define MULIB_WEBSEARCH_PROVIDER_H
#include "mulib-browser-origin.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_websearch_provider_config
  {
    char name[32], endpoint[512];
    unsigned max_results;
  } mulib_websearch_provider_config_t;
  /** @brief See manual/mulib-websearch-provider.md for this operation's
   * contract. */
  int mulib_websearch_provider_init (mulib_websearch_provider_config_t *out,
                                     const char *name, const char *endpoint,
                                     unsigned limit);
  /** @brief See manual/mulib-websearch-provider.md for this operation's
   * contract. */
  int mulib_websearch_provider_url (const mulib_websearch_provider_config_t *provider,
                                    const char *query, unsigned offset,
                                    mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBSEARCH_PROVIDER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-websearch-provider.h`](../include/mulib-websearch-provider.h) |
| Implementation switch | `MULIB_IMPL_WEBSEARCH_PROVIDER` |
| Repository-local includes | [mulib-browser-origin](mulib-browser-origin.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/websearch-provider.c`](../examples/modules/websearch-provider.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/websearch-provider.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
