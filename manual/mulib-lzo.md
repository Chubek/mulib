# mulib-lzo

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

Original LZO1X raw bytecode, shared by lzop. Define `MULIB_IMPL_LZO`,
`MULIB_IMPL_CODEC_CORE` and `MULIB_IMPL_BUFFER` in one translation unit.
[Complete example](../examples/modules/lzo.c).

`mulib_lzo1x_encode` uses a bounded 8192-entry greedy matcher, distances up to
16384, matches up to 65536 bytes, variable-length M3 match commands, initial/long
literal runs and trailing literals. It emits the standard `11 00 00` EOS marker.
It handles arbitrary bytes and empty input, not merely literal-only bytecode.

`mulib_lzo1x_decode` implements all original LZO1X short/medium/long distance and
length forms, state-dependent short matches, initial literals and following
literals. It validates distance/history, integer overflow, input availability,
output limit and exact EOS, rejecting trailing bytes. Kernel LZO-RLE version-1
zero-run extensions are not accepted. There is no checksum in raw LZO1X.

Both functions borrow input, allow input/output aliasing and append atomically.
Results and aggregate decoder limit are as described in the common contracts.
Encoding uses a hash table plus a match-token array proportional to input; decoding
uses only bounded staged output. Running time is linear, with bounded greedy match
search. Independent liblzo2 encode/decode checks are optional test oracles, never
runtime dependencies. Truncation, mutation, expansion and allocation-failure tests
run in the focused compression suites.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lzo.h`](../include/mulib-lzo.h) |
| Implementation switch | `MULIB_IMPL_LZO` |
| Repository-local includes | [mulib-codec-core](mulib-codec-core.md) |
| Complete example | [`examples/modules/lzo.c`](../examples/modules/lzo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lzo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
