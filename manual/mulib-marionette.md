# mulib-marionette

[Manuals](README.md) · [Library index](index.md)

Bounded input recording and deterministic callback playback. No scripting engine,
file format, sleeping, global clock, browser protocol or automatic input injection.

## Integration and storage

Define `MULIB_IMPL_INPUT`, `MULIB_IMPL_VEC`, `MULIB_IMPL_MARIONETTE` before
including the header in one C11 translation unit. Initialize with
`mulib_marionette_init(&recording,limit)`; zero limit selects 1,000,000 events.
Storage is an owned MuLIB vector; never shallow-copy a recording or edit fields.
`free` releases/reset storage and is repeatable for initialized objects.

`append(&recording,&event)` copies valid events with nondecreasing time offsets
in microseconds from playback start. Equal timestamps retain insertion order.
Returns 1 on success, 0 for invalid/time-order/full/allocation failure or playback
in progress; previous events remain intact. Normalize source-device timestamps
to relative offsets yourself. No string parsing or retained source pointers.

## Caller-clocked scheduling

`start(&recording,start_us)` restarts at cursor zero; returns 0 without mutation
if start plus the last offset would overflow uint64_t. Empty recordings succeed
and are immediately finished. `tick(&recording,now_us,budget,sink,user,&sent)`
emits due events in order, up to budget (zero means unlimited). Returns 0 on
success, -1 invalid, -2 sink rejection. `sent` reports accepted events even on
partial failure. Sink returns zero after acceptance; failed event remains pending
and may be retried. Sink must not mutate/reenter the same recording. Event
timestamps remain offsets, not rewritten to caller clock values.

Backward times do not rewind previously accepted events or emit future ones.
`stop` only stops scheduling; it emits no releases. Consumers injecting real
events must track/release held keys on cancellation/error and preserve SYN frames.
No concurrency guarantee; callbacks can forward to `mulib-vdev` explicitly.

`cc -std=c11 -Iinclude examples/modules/marionette.c -o marionette`.
Tests cover limits, empty playback, clock/overflow boundaries and sink retry.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-marionette.h`](../include/mulib-marionette.h) |
| Implementation switch | `MULIB_IMPL_MARIONETTE` |
| Repository-local includes | [mulib-input](mulib-input.md), [mulib-vec](mulib-vec.md) |
| Complete example | [`examples/modules/marionette.c`](../examples/modules/marionette.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/marionette.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
