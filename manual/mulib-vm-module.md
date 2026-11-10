# MuLIB vm-module

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/vm-module.c)

`add` verifies a nonempty bytecode program with the supplied local count, then
copies its nonempty name and instruction array. Duplicate names, verifier
failures, invalid arguments, and allocation failure leave the registry unchanged.
Verification is structural; runtime arithmetic and budget failures remain possible.
Enable BYTECODE_VERIFY along with VM_MODULE (and BYTECODE when executing).

`find` performs exact name lookup and returns a borrowed module or NULL. Registry
growth may invalidate the module descriptor pointer; owned code/name bytes remain
until destruction. This API registers in-memory instructions, not serialized bytes;
decode with bytecode-serialize first. `modules_free` releases all copies. Lookup
is linear; add includes O(code count) verification/copying.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-vm-module.h`](../include/mulib-vm-module.h) |
| Implementation switch | `MULIB_IMPL_VM_MODULE` |
| Repository-local includes | [mulib-bytecode-verify](mulib-bytecode-verify.md), [mulib-bytecode](mulib-bytecode.md) |
| Complete example | [`examples/modules/vm-module.c`](../examples/modules/vm-module.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vm-module.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
