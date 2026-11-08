# mulib-dfa

[Manuals](README.md) · [Library index](index.md)

Deterministic finite automata over inclusive `uint32_t` input ranges, plus actual
epsilon-NFA subset construction. Define `MULIB_IMPL_DFA`, `MULIB_IMPL_FST`,
`MULIB_IMPL_FST_SERIALIZE`, `MULIB_IMPL_VEC`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_ENDIAN`, and `MULIB_IMPL_CRC32` in one C11 translation unit. The
FST serialization switch is opt-in so existing core FST clients keep their
original dependency requirements.

## Building and running

`create(max_states, max_edges)` returns an empty owned graph. `add_state` copies
an accepting flag and returns a stable state ID; the first state becomes the
default start. Use `set_start` to override it. `add_range` rejects overlapping
outgoing ranges, even when destinations agree. `graph` borrows the underlying
read-only output-free FST. `start` returns its start ID.

`accepts(dfa, tokens, count, work)` consumes the entire symbol sequence. Return
codes are FST codes: zero accepts, `MULIB_FST_NOMATCH` rejects, negative codes
report invalid input, allocation failure, conflicts or limits. A missing
transition rejects; no explicit sink state is required. Accepting start states
can recognize the empty sequence. No character encoding is inferred: callers
choose bytes, Unicode scalars, lexer tokens, or other uint32 symbols.

## NFA conversion

Provide a borrowed `mulib_nfa_t`: state count, start, accepting-byte array and
edges `{from,to,lower,upper,epsilon}`. Epsilon edges do not consume symbols;
their range fields are ignored. Overlapping and duplicate NFA ranges are legal.

`mulib_dfa_from_nfa` computes the start epsilon closure, partitions the alphabet
at every range endpoint (including the 2^32 sentinel), then explores reachable
subsets. Each destination receives full transitive epsilon closure, including
cycles. A subset accepts if any member accepts. Empty subsets become missing
edges. Conversion is iterative and bounded; it is not an NFA simulator disguised
as a DFA. Equivalent subsets share a DFA state. Minimization is not performed.

NFA caps are 4096 states and 65536 edges. Zero options select 4096 output states,
65536 edges and 10000000 conversion work units. Subset storage is capped at
64 MiB; exponential state growth returns LIMIT and frees the partial graph.
`create` itself uses the underlying FST's graph-capacity defaults when zero.

## Snapshots

`serialize` atomically appends a versioned little-endian start/graph snapshot
with CRC. `deserialize` validates the entire document, start, graph ranges,
accepting bits and absence of outputs, then returns a new owned DFA. It rejects
trailing bytes; output pointers are unchanged on error. State/edge IDs are
preserved, linked-list addresses are not serialized. `destroy(NULL)` is safe.
CRC is not authentication. Use `mulib-cache` for explicit indexed disk caching.

See [`examples/modules/dfa.c`](../examples/modules/dfa.c) and
`tests/test_hyphen.c` for epsilon cycles, overlapping ranges, UINT32_MAX labels,
serialization, corruption and state-limit tests.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dfa.h`](../include/mulib-dfa.h) |
| Implementation switch | `MULIB_IMPL_DFA` |
| Repository-local includes | [mulib-crc32](mulib-crc32.md), [mulib-endian](mulib-endian.md), [mulib-fst](mulib-fst.md) |
| Complete example | [`examples/modules/dfa.c`](../examples/modules/dfa.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dfa.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
