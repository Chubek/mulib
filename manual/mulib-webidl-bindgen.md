# webidl-bindgen

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

C header declaration generation from a bounded interface-name WebIDL subset.

## Contract and supported profile

Generate opaque C typedef handles for `interface Name;` and `interface Name
{ ... };` declarations. Names begin with a letter/underscore, continue with
alphanumeric/underscore and are <=80 bytes. Whitespace and C-style line/block
comments are skipped; quoted body text and balanced braces (depth<=64) are
respected. Members are skipped, not translated or semantically checked.
Inheritance, partial interfaces, dictionaries, mixins and top-level extensions
are rejected. Input<=4 MiB, no embedded NUL; max_interfaces is an explicit count
budget (zero allows only empty input). Output appends atomically.

## Integration and ownership

Include `include/mulib-webidl-bindgen.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_WEBIDL_BINDGEN`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/webidl-bindgen.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-webidl-bindgen.h`](../include/mulib-webidl-bindgen.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-webidl-bindgen.h
 * @brief C header declaration generation from a bounded interface-name WebIDL
 * subset. C11 single-header module. Define MULIB_IMPL_WEBIDL_BINDGEN in
 * exactly one C translation unit. Generates declaration handles, not full
 * WebIDL binding semantics.
 */
#ifndef MULIB_WEBIDL_BINDGEN_H
#define MULIB_WEBIDL_BINDGEN_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-webidl-bindgen.md for this operation's contract.
   */
  int mulib_webidl_bindgen_interfaces (const char *idl, size_t n,
                                       mulib_shylock_buffer_t *out,
                                       size_t max_interfaces);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBIDL_BINDGEN_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webidl-bindgen.h`](../include/mulib-webidl-bindgen.h) |
| Implementation switch | `MULIB_IMPL_WEBIDL_BINDGEN` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/webidl-bindgen.c`](../examples/modules/webidl-bindgen.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webidl-bindgen.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
