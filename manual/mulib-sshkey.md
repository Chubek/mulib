# sshkey — SSH Ed25519 blobs

Encode or parse SSH `ssh-ed25519` public key and signature blobs. The public
key and signature byte spans returned by parsers borrow the input. These
helpers do not verify signatures and do not read OpenSSH private key files or
`authorized_keys` lines.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sshkey.h`](../kernels/mulib-sshkey.h) |
| Implementation switch | `MULIB_IMPL_SSHKEY` |
| Repository-local includes | [mulib-sshwire](mulib-sshwire.md) |
| Complete example | [`examples/ssh/kernels.c`](../examples/ssh/kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/ssh/kernels.c -pthread -lm -ldl -lcrypto -o mulib-example
```
<!-- /manual-reference -->
