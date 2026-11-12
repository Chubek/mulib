# webextract-llm

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

OpenAI-compatible JSON extraction request builder and pluggable response transport.

## Contract and supported profile

Request builds an OpenAI Responses-style JSON body with escaped model/page
strings and an exact JSON-object schema validated by MuLIB JSON. Model is
nonempty and <=128 bytes, schema<=16384, text<=max_text and 16 MiB. It does not
validate JSON Schema semantics. Call invokes a synchronous borrowed transport
on a nonempty request<=16 MiB, stages its response and enforces response.limit
(default 16 MiB) before appending. Transport must honor the supplied buffer and
not retain it. Failure preserves caller output. Authentication, HTTPS, response
schema/evidence validation and prompt-injection defense remain caller obligations.

## Integration and ownership

Include `include/mulib-webextract-llm.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_WEBEXTRACT_LLM`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/webextract-llm.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-webextract-llm.h`](../include/mulib-webextract-llm.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-webextract-llm.h
 * @brief OpenAI-compatible JSON extraction request builder and pluggable
 * response transport. C11 single-header module. Define
 * MULIB_IMPL_WEBEXTRACT_LLM in exactly one C translation unit. The caller
 * provides a real authenticated HTTPS transport and verifies response schemas.
 */
#ifndef MULIB_WEBEXTRACT_LLM_H
#define MULIB_WEBEXTRACT_LLM_H
#include "mulib-json.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_webextract_llm_transport_fn) (
      void *ctx, const char *request_json, size_t length,
      mulib_shylock_buffer_t *response);
  /** @brief See manual/mulib-webextract-llm.md for this operation's contract.
   */
  int mulib_webextract_llm_request (const char *model, const char *text,
                                    const char *schema, size_t max_text,
                                    mulib_shylock_buffer_t *out);
  /** @brief See manual/mulib-webextract-llm.md for this operation's contract.
   */
  int mulib_webextract_llm_call (mulib_webextract_llm_transport_fn transport,
                                 void *ctx, const char *request, size_t n,
                                 mulib_shylock_buffer_t *response);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBEXTRACT_LLM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webextract-llm.h`](../include/mulib-webextract-llm.h) |
| Implementation switch | `MULIB_IMPL_WEBEXTRACT_LLM` |
| Repository-local includes | [mulib-json](mulib-json.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/webextract-llm.c`](../examples/modules/webextract-llm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webextract-llm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
