# mulib-keyremap

[Manuals](README.md) · [Library index](index.md)

Stateful, one-event-at-a-time key-code mapping over `mulib-input`. No textual
configuration language, modifiers, chords, macros, key names or device injection.

## Integration and lifetime

Define `MULIB_IMPL_INPUT` and `MULIB_IMPL_KEYREMAP` in one C11 translation unit.
`mulib_keyremap_init(&remap)` establishes identity mappings and clears held state.
State is fixed-size, value-owned, allocation-free (roughly 5.4 KiB). No cleanup
is required. Do not reset or discard held state before emitting releases to a
downstream virtual device. State is independent per instance; no global mappings.

`set(&remap,source,target)` accepts codes 0..767 or target
`MULIB_KEYREMAP_DROP`. It returns 1 on success, 0 on invalid arguments. Changes
take effect at the next press, not on already-held keys.

## Event semantics

`apply(&remap,&event,&out)` returns 1 for an output event, 0 for suppression, -1
for invalid input without mutation. Output is untouched on 0/-1 and may alias
input. Non-key events pass through. Presses capture the current target; repeats
and releases continue to use that target even after mapping changes. Duplicate
presses and orphan releases/repeats are suppressed.

Many source keys may target one key: only the first press and last release are
emitted. Held-key repeats still pass through. Dropped keys produce no events.
SYN_REPORT passes through even for an empty mapped frame. SYN_DROPPED is not
automatically recoverable: stop forwarding, reconcile native held state and
release/reset downstream state before resuming. No callback failures to roll back.

`cc -std=c11 -Iinclude examples/modules/keyremap.c -o keyremap`.
`tests/test_devices.c` checks many-to-one balancing, remapping while held,
suppression, aliasing, drops and malformed events.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-keyremap.h`](../include/mulib-keyremap.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-keyremap.h
 * @brief Stateful key-code remapping with balanced many-to-one key releases.
 * Define MULIB_IMPL_KEYREMAP and MULIB_IMPL_INPUT once.
 */
#ifndef MULIB_KEYREMAP_H
#define MULIB_KEYREMAP_H
#include "mulib-input.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_KEYREMAP_DROP UINT16_MAX
  /** @brief Value-owned state; initialize before use. Changes apply at next
   * key press. */
  typedef struct mulib_keyremap
  {
    uint16_t mapping[MULIB_INPUT_KEY_COUNT], active[MULIB_INPUT_KEY_COUNT],
        references[MULIB_INPUT_KEY_COUNT];
    unsigned char pressed[MULIB_INPUT_KEY_COUNT];
  } mulib_keyremap_t;
  /** @brief Identity mapping and no held keys; resetting does not emit
   * releases. */
  void mulib_keyremap_init (mulib_keyremap_t *remap);
  /** @brief Map a key to another key or DROP; return 1 on success, 0 if
   * invalid. */
  int mulib_keyremap_set (mulib_keyremap_t *remap, unsigned source,
                          unsigned target);
  /** @brief Consume one event: 1 output, 0 suppressed, -1 invalid (no
   * mutation). Non-key events pass through. Duplicate presses and orphan
   * releases/repeats are suppressed. Output may alias input. SYN_DROPPED
   * requires caller recovery.
   */
  int mulib_keyremap_apply (mulib_keyremap_t *remap,
                            const mulib_input_event_t *event,
                            mulib_input_event_t *out);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-keyremap.h`](../include/mulib-keyremap.h) |
| Implementation switch | `MULIB_IMPL_KEYREMAP` |
| Repository-local includes | [mulib-input](mulib-input.md) |
| Complete example | [`examples/modules/keyremap.c`](../examples/modules/keyremap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/keyremap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
