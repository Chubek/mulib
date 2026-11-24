# MuLIB kernel import examples

These examples use the source syntax retained by the
[FFI migration](../../docs/ffi-migration.md). The reference build commands
below are in the linked guides; this directory does not contain generated
bindings or production review evidence.

| Example | Build requirements |
| --- | --- |
| [`clamp.on1x`](clamp.on1x) | [Unsafe static clamp](../../docs/kernel-bind.md#existing-static-build). The three calls require lexical `unsafe`; the interval check still runs. |
| [`clamp_safe.on1x`](clamp_safe.on1x) | [Independently reviewed clamp](../../docs/kernel-review.md#reproducible-workflow). A candidate report alone will not authorize the build. |
| [`forecast.on1x`](forecast.on1x), [`pricing/main.on1x`](pricing/main.on1x) | [Reviewed numerical profiles](../../docs/kernel-numerics.md). The combined example needs review for the complete selected module set. |
| [`framed_pair.on1x`](framed_pair.on1x), [`framed_session.on1x`](framed_session.on1x) | [Unsafe local framed transport](../../docs/kernel-framed.md). These use a bounded native companion, not a general pointer or callback FFI. |

The `[dynlib]` variant uses the same call syntax, but requires a qualified
shared build. For a reviewed import it also needs a review specific to that
shared artifact. See [dynamic kernel linkage](../../docs/kernel-bind.md#compile-the-implementation-as-a-shared-library).
