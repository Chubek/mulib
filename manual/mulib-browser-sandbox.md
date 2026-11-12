# browser-sandbox

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Deterministic resource-budget accounting and abort flags for browser tasks.

## Contract and supported profile

Init copies validated limits into fresh value storage and zeroes counters.
Charge atomically adds response bytes, nodes, script steps and tasks only if all
remain within bounds. It rejects corrupt over-budget counters before subtraction,
avoiding unsigned underflow, and preserves counters on failure. Cancel sets an
irreversible flag until reinitialization; cancelled charges fail even for zero.
This is cooperative accounting, not memory interception, execution preemption,
time limits or an OS/process sandbox. Callers must charge every relevant action
and synchronize shared state externally.

## Integration and ownership

Include `include/mulib-browser-sandbox.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_LIMITS`, `MULIB_IMPL_BROWSER_SANDBOX`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-sandbox.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-sandbox.h`](../include/mulib-browser-sandbox.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-sandbox.h
 * @brief Deterministic resource-budget accounting and abort flags for browser
 * tasks. C11 single-header module. Define MULIB_IMPL_BROWSER_SANDBOX in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_SANDBOX_H
#define MULIB_BROWSER_SANDBOX_H
#include "mulib-browser-limits.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_sandbox
  {
    mulib_browser_limits_t limits;
    size_t response_bytes, nodes, script_steps, tasks;
    int cancelled;
  } mulib_browser_sandbox_t;
  /** @brief See manual/mulib-browser-sandbox.md for this operation's contract.
   */
  int mulib_browser_sandbox_init (mulib_browser_sandbox_t *s,
                                  const mulib_browser_limits_t *limits);
  /** @brief See manual/mulib-browser-sandbox.md for this operation's contract.
   */
  int mulib_browser_sandbox_charge (mulib_browser_sandbox_t *s, size_t bytes,
                                    size_t nodes, size_t steps, size_t tasks);
  /** @brief See manual/mulib-browser-sandbox.md for this operation's contract.
   */
  void mulib_browser_sandbox_cancel (mulib_browser_sandbox_t *s);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_SANDBOX_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-sandbox.h`](../include/mulib-browser-sandbox.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_SANDBOX` |
| Repository-local includes | [mulib-browser-limits](mulib-browser-limits.md) |
| Complete example | [`examples/modules/browser-sandbox.c`](../examples/modules/browser-sandbox.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-sandbox.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
