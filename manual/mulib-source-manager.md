# MuLIB source-manager

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/source-manager.c)

`set` copies explicit-length bytes, appends a convenience NUL, and indexes LF
line starts. Embedded NULs are preserved. Zero-length NULL input creates one empty
line. Replacement is atomic on failure and can use bytes from the current source.
Zero-initialize before first set; `free` releases both bytes and line index.

`position` maps offsets 0..length inclusive to one-based line and byte column.
An LF belongs to the preceding line; a trailing LF starts an empty final line.
CR is an ordinary byte, so CRLF advances only at LF. Columns are not Unicode
character or display widths. Invalid offsets/uninitialized source leave outputs
unchanged. Set takes O(bytes), lookup O(log lines), storage O(bytes + lines).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-source-manager.h`](../include/mulib-source-manager.h) |
| Implementation switch | `MULIB_IMPL_SOURCE_MANAGER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/source-manager.c`](../examples/modules/source-manager.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/source-manager.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
