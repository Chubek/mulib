# MuLIB gvn

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/gvn.c)

`intern` maps an opcode and two uint64 operands to a stable size_t value number.
With `commutative != 0`, operands are ordered before lookup. Repeated keys return
the same ID; new keys append. The caller assigns opcode meaning and must use a
consistent commutativity policy for each opcode. Failures preserve output IDs.

The table does not inspect side effects, memory aliasing, control flow, or types.
Use it only for pure expressions whose operand identities and opcode fully
describe their semantics, and reset it when those identities cease to be valid.
Zero-initialize and `free` after use. Linear lookup gives O(entries) insertion
time and O(entries) storage.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-gvn.h`](../include/mulib-gvn.h) |
| Implementation switch | `MULIB_IMPL_GVN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/gvn.c`](../examples/modules/gvn.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/gvn.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
