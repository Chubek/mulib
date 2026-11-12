# jsdom

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Native callback bridge for script-visible DOM object/property operations.

## Contract and supported profile

Invoke validates GET/SET/CALL, nonzero object ID, nonempty property<=256
bytes, a valid optional input span and nonempty output capacity. It borrows
dispatch/context, stages output in an allocated zeroed buffer and requires a
terminated result. Nonzero callback success becomes one; failure preserves
caller output, but native callback side effects are not rolled back. The bridge
owns no object table or JavaScript values. The host must authorize IDs/property
access and keep contexts alive; this is not a JS interpreter or a sandbox.

## Integration and ownership

Include `include/mulib-jsdom.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_JSDOM`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/jsdom.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-jsdom.h`](../include/mulib-jsdom.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-jsdom.h
 * @brief Native callback bridge for script-visible DOM object/property
 * operations. C11 single-header module. Define MULIB_IMPL_JSDOM in exactly one
 * C translation unit. Callback bridge substrate, not native ECMAScript
 * property descriptors.
 */
#ifndef MULIB_JSDOM_H
#define MULIB_JSDOM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_jsdom_op
  {
    MULIB_JSDOM_GET = 1,
    MULIB_JSDOM_SET = 2,
    MULIB_JSDOM_CALL = 3
  } mulib_jsdom_op_t;
  typedef int (*mulib_jsdom_dispatch_fn) (void *ctx, mulib_jsdom_op_t op,
                                          uint64_t object_id,
                                          const char *property,
                                          const char *input, size_t length,
                                          char *out, size_t cap);
  typedef struct mulib_jsdom
  {
    mulib_jsdom_dispatch_fn dispatch;
    void *ctx;
  } mulib_jsdom_t;
  /** @brief See manual/mulib-jsdom.md for this operation's contract. */
  int mulib_jsdom_invoke (const mulib_jsdom_t *js, mulib_jsdom_op_t op,
                          uint64_t object_id, const char *property,
                          const char *value, size_t n, char *out, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_JSDOM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-jsdom.h`](../include/mulib-jsdom.h) |
| Implementation switch | `MULIB_IMPL_JSDOM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/jsdom.c`](../examples/modules/jsdom.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/jsdom.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
