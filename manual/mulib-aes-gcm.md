# aes-gcm — AES-128-GCM authenticated encryption

**Header:** `include/mulib-aes-gcm.h` · **Switch:** `MULIB_IMPL_AES_GCM`

Also define `MULIB_IMPL_ENTROPY`. This module implements NIST SP 800-38D with
16-byte AES keys, 12-byte nonces and 16-byte authentication tags. It does not
change the existing `mulib-aes.h` AES-256/CTR message format.

Initialize `mulib_aes_gcm_t` with `mulib_aes_gcm_init(&ctx,key)`. The context owns
expanded key material; use `mulib_aes_gcm_clear` when finished. Init returns 1/0
and leaves the context unchanged for invalid pointers. It may overwrite a prior
context. Const contexts may be used concurrently with distinct output buffers.

`seal(ctx,nonce,aad,aad_length,plain,length,cipher,tag)` encrypts and authenticates
both AAD and ciphertext. **Never reuse a nonce with the same key.** The module
does not allocate nonces or enforce a per-key message budget. TLS assigns nonces
using its traffic IV and sequence number and bounds each traffic key's usage.

`open(ctx,nonce,aad,aad_length,cipher,length,tag,plain)` authenticates before
writing plaintext. A bad tag returns 0 with output unchanged. Both operations
return 1 on success and 0 on invalid arguments. Payloads may be empty (NULL is
valid then), and length must not exceed `2^36 - 32` bytes. AAD bit length must fit
in 64 bits. Exact in-place input/output is supported; other overlap is forbidden.
Context, keying inputs, AAD and tag must be disjoint from output. Seal's tag must
also be disjoint from its ciphertext and borrowed inputs.

The portable AES S-box uses algebraic operations rather than secret-indexed
tables; GHASH uses fixed-schedule field multiplication. GCC/Clang x86 builds
dispatch to AES-NI after a CPU feature check, without requiring global `-maes`.
Define `MULIB_AES_GCM_NO_INTRINSICS` to force portable AES.

[`examples/modules/aes-gcm.c`](../examples/modules/aes-gcm.c) demonstrates seal,
open and altered-tag rejection. NIST empty/one-block/AAD/partial-block vectors
and portable/hardware paths are covered by `tests/test-tls-crypto.c`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-aes-gcm.h`](../include/mulib-aes-gcm.h) |
| Implementation switch | `MULIB_IMPL_AES_GCM` |
| Repository-local includes | [mulib-entropy](mulib-entropy.md) |
| Complete example | [`examples/modules/aes-gcm.c`](../examples/modules/aes-gcm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/aes-gcm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
