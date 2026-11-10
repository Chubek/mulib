# MuLIB liveness

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/liveness.c)

`compute` solves `out[b] = union(in[successor])` and
`in[b] = use[b] | (out[b] & ~def[b])` to a fixed point for up to 64 blocks and
64 register bits. `use` must contain uses before definitions in each block;
the caller derives these summaries. All blocks, including disconnected ones,
participate. No implicit live-out bits are seeded at exits.

Result copies use/def arrays and provides in/out bitsets and block count. It is
a value object and remains unchanged on failure. Invalid CFG endpoints are
rejected before analysis. Zero-block graphs accept NULL use/def arrays. Work is
bounded by monotone bit propagation, with fixed-size 64-block result/scratch.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-liveness.h`](../include/mulib-liveness.h) |
| Implementation switch | `MULIB_IMPL_LIVENESS` |
| Repository-local includes | [mulib-cfg](mulib-cfg.md) |
| Complete example | [`examples/modules/liveness.c`](../examples/modules/liveness.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/liveness.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
