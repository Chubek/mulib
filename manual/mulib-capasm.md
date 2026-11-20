# CAPASM — C as Portable Assembly

CAPASM lowers validated [XMacIR](mulib-xmacir.md) to ISO C90 (ANSI C).
The C kernel itself builds as C11. Generated source uses neither GNU
extensions nor C99 headers, syntax or types. The C90 output is portable
subject to the target compiler's translation limits and matching C ABI
for any host bridge it calls.

A scalar program returns an `unsigned long` in `[0, 4294967295]`, even
when the target's `unsigned long` is wider. A capsule exports a normal C
function with the specified struct-pointer parameters and calls its
bridge with immutable payload bytes. Generated capsules do not perform
I/O or load any plugin.

```sh
cc -std=c11 -Iinclude examples/modules/capasm.c -o capasm-example
./capasm-example > answer.c
cc -std=c90 -pedantic-errors -c answer.c -o answer.o
```

Emission without options requires no external tools. Hosts can select
named m4 rewrite passes and supply the tool runner and toolchain. The
runner receives explicit arguments, not a shell command. Rewrite
programs and tool executables are **trusted build inputs**; never add
untrusted directories to `XMACIR_REWRITE_PATH`. Rules are not sandboxed.
See the [bundled rewrite library](../library/xmacir-trs/README.md) and
[Termscript](mulib-termscript.md) for a complete consumer.

`mulib_capasm_emit` revalidates each rewrite, rejecting changes to exported
names, capsule bytes, or the scalar program's result. With configured build
stages, it returns the final stage's artifact bytes, not necessarily C
text. The kernel does not resolve paths, execute a tool by itself, or
verify an object or executable produced by a tool.

Set `mulib_capasm_options_t.kernels` and `.kernel_count` to include MuLIB
headers in first-occurrence order. For example, `"money"` emits
`#include "mulib-money.h"`. CAPASM and XMacIR themselves cannot be selected.
CAPASM emits no implementation switches and does not check whether a
header exists, resolve transitive dependencies, or guarantee that arbitrary
headers compose. Supply the compiler include path and the required
implementation translation units separately. The emitted body is still
C90, but the complete file must use the selected headers' C dialect.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-capasm.h`](../include/mulib-capasm.h) |
| Implementation switch | `MULIB_IMPL_CAPASM` |
| Repository-local includes | [mulib-xmacir](mulib-xmacir.md) |
| Complete example | [`examples/modules/capasm.c`](../examples/modules/capasm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/capasm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
