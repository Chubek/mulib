# schemaorg

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Bounded extraction of plain Product/Offer JSON-LD scalar fields.

## Contract and supported profile

Parse validates one complete UTF-8 JSON object of at most 4 MiB using MuLIB
JSON. It supports a flat offer or a Product-like object with a single `offers`
object; top-level name is optional, priceCurrency is required uppercase three
letters, price is a nonnegative decimal string or finite JSON number, and
availability is an optional string. Escapes are decoded; embedded NUL, wrong
types and oversized fields fail. Output is a copied value and unchanged on
failure. No arrays, @graph, linked contexts, type inference or JSON-LD expansion.
Numeric JSON prices pass through binary floating point; use strings for precision.

## Integration and ownership

Include `include/mulib-schemaorg.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_SCHEMAORG`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/schemaorg.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-schemaorg.h`](../include/mulib-schemaorg.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-schemaorg.h
 * @brief Bounded extraction of plain Product/Offer JSON-LD scalar fields.
 * C11 single-header module. Define MULIB_IMPL_SCHEMAORG in exactly one C
 * translation unit. Supports a flat/scalar extraction profile; not a complete
 * JSON-LD or Schema.org processor.
 */
#ifndef MULIB_SCHEMAORG_H
#define MULIB_SCHEMAORG_H
#include "mulib-json.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_schemaorg_offer
  {
    char name[256], currency[8], price[48], availability[128];
  } mulib_schemaorg_offer_t;
  /** @brief See manual/mulib-schemaorg.md for this operation's contract. */
  int mulib_schemaorg_offer_parse (const char *json, size_t n,
                                   mulib_schemaorg_offer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SCHEMAORG_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-schemaorg.h`](../include/mulib-schemaorg.h) |
| Implementation switch | `MULIB_IMPL_SCHEMAORG` |
| Repository-local includes | [mulib-json](mulib-json.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/schemaorg.c`](../examples/modules/schemaorg.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/schemaorg.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
