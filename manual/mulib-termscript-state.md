# termscript-state — Finite state machines

The C11 transition engine borrows an immutable table sorted in ascending
`(source, event)` order. The machine checks the table when initialized and
finds each transition by binary search. Negative state and event IDs are
supported. A missing transition leaves the state unchanged.

An optional callback decides whether to commit a transition. It sees the
previous state and cannot send another event recursively. External effects
performed by an action are not rolled back.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-termscript-state.h`](../kernels/mulib-termscript-state.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_STATE` |
| Repository-local includes | None |
| Complete example | [`examples/termscript/support.c`](../examples/termscript/support.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/termscript/support.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
