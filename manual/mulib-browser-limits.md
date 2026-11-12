# browser-limits

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Validated resource limits for untrusted document execution.

## Contract and supported profile

Default writes budgets of 16 MiB response bytes, 100000 nodes, depth 128,
10000 tasks, one million script steps and 10000 URLs. Valid requires response
bytes 1024..512 MiB, nodes 1..10000000, depth 1..1024, tasks 1..1000000, and
nonzero script-step/URL budgets. These are copied configuration values, not an
automatic enforcement mechanism. Browser-engine enforces response/node/depth/
history limits; sandbox explicitly accounts charged resources. No process memory,
wall-clock or OS security guarantee follows from validation.

For HTML loading, max_depth uses the tree builder's element-depth convention;
the synthetic document root and text nodes do not consume element depth.

## Integration and ownership

Include `include/mulib-browser-limits.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_LIMITS`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-limits.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-limits.h`](../include/mulib-browser-limits.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
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
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-limits.h`](../include/mulib-browser-limits.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_LIMITS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browser-limits.c`](../examples/modules/browser-limits.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-limits.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
