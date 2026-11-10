# MuLIB diagnostic

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/diagnostic.c)

`add` copies a NUL-terminated message and appends code, severity, source ID, and
byte span in insertion order. Severity is NOTE, WARNING, or ERROR; `errors` counts
ERROR records. Source IDs/codes are application-defined, spans are half-open,
and start+length overflow is rejected. This module does not consult source bytes.

Zero-initialize the list. Failed insertion leaves records and error count intact.
The message pointer is owned by the list; array element pointers may move on
growth. `free` releases messages and array storage and resets counters.
Insertion costs O(message length) amortized; no sorting or rendering is performed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-diagnostic.h`](../include/mulib-diagnostic.h) |
| Implementation switch | `MULIB_IMPL_DIAGNOSTIC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/diagnostic.c`](../examples/modules/diagnostic.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/diagnostic.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
