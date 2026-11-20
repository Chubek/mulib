# DecisionTrace — Structured decision evidence

Append events to caller-owned storage and use `mulib_decisiontrace_encode_v1`
to encode fields without copying struct padding. The FNV fingerprint is a
deterministic comparison aid, not an authentication primitive. Persistent
evidence must separately bind exact input digests, currency, policy IDs,
and results. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-decisiontrace.h`](../include/mulib-decisiontrace.h) |
| Implementation switch | `MULIB_IMPL_DECISIONTRACE` |
| Repository-local includes | None |
| Complete example | [`examples/hospitality_demo.c`](../examples/hospitality_demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/hospitality_demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
