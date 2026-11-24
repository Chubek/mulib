# XMacIR — Bounded X Macro interchange

XMacIR serializes portable programs as a restricted stream of X macro
invocations. It is **not arbitrary C source or an m4 program**. Parse and
validate it before sending it to tools. The canonical formatter supplies
stable decimal integers, hexadecimal bytes, and line boundaries.

- `XM_U32(entry)` starts a stack program. `XM_PUSH(n)` pushes an unsigned
  32-bit constant. `XM_ADD()`, `XM_SUB()`, and `XM_MUL()` wrap modulo 2³²;
  `XM_AND()`, `XM_OR()`, and `XM_XOR()` are bitwise. `XM_DUP()`, `XM_DROP()`,
  and `XM_SWAP()` manipulate the stack. `XM_RET()` must end the program
  with exactly one stack value.
- `XM_BLOB(entry,bridge,context_tag,error_tag)` starts a capsule. Its
  `XM_BYTES(h...)` rows contain even-length lowercase hexadecimal, up to
  32 decoded bytes per row. `XM_END()` closes the capsule. The bridge is
  a named C function supplied by the caller at link time, not a pointer
  or expression encoded in the IR.

The parser caps scalar programs at 65,536 instructions and 256 stack
values. Capsules hold up to 6 MiB, and input text is capped at 16 MiB.
Names are restricted C identifiers; the `XM_` and `m4_` namespaces are
reserved so validated input cannot name the bundled rules' macros.

For example:

```text
XM_U32(portable_answer)
XM_PUSH(6)
XM_PUSH(7)
XM_MUL()
XM_RET()
```

See [CAPASM](mulib-capasm.md) for emission, rewrite passes, and tools.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-xmacir.h`](../kernels/mulib-xmacir.h) |
| Implementation switch | `MULIB_IMPL_XMACIR` |
| Repository-local includes | None |
| Complete example | [`examples/modules/capasm.c`](../examples/modules/capasm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/capasm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
