# Contract — Versioned content envelope

Use `mulib_contract_valid` and `mulib_contract_usable_at` to check an
in-memory envelope before interpreting its payload. Times are nonnegative
Unix seconds; validity ends at the exclusive `valid_until`. The caller
owns the strings and payload and must keep them readable through the call.
This kernel does not resolve dataset references or validate payload schemas.

`mulib_contract_digest_sha256` identifies exact envelope content in a
versioned encoding; it does not authenticate who supplied the data.
The FNV-1a fingerprint is noncryptographic and not a substitute.
See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-contract.h`](../kernels/mulib-contract.h) |
| Implementation switch | `MULIB_IMPL_CONTRACT` |
| Repository-local includes | [mulib-sha](mulib-sha.md) |
| Complete example | [`examples/hospitality_demo.c`](../examples/hospitality_demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/hospitality_demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
