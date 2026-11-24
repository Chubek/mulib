# sshtransport — Pinned SSH-2 transport

The supported suite is `curve25519-sha256`, `ssh-ed25519`,
`aes128-gcm@openssh.com` in both directions, and no compression. Define the
switches shown in the [runnable in-memory example](../examples/ssh/kernels.c).
Link OpenSSL `libcrypto` (1.1.1 or newer). Run `make check-ssh` or enable
`MULIB_BUILD_SSH_OPENSSL` in CMake; default builds do not require OpenSSL.
`make check-ssh-openssh` tests both directions with explicit pins and a local
OpenSSH client and server. It needs `ssh`, `sshd`, `ssh-keygen`, and a local
account that the system's `sshd` permits for public-key authentication. To
enable this fixture under CTest on Linux, also set `MULIB_BUILD_SSH_OPENSSH_TESTS`.
Set `MULIB_BUILD_FUZZER` with Clang and the OpenSSL option for the SSH parser
fuzz targets.

Exchange identification lines, then start the role with the exact line bytes
without CRLF. The client must supply a trusted Ed25519 public key pin; the
server supplies its persistent private seed. Keep emitted records in order,
including when a batch contains more than one frame. Retain incomplete reads
and writes. Use `mulib_sshtransport_handshake` for the initial exchange; once
ready, seal outgoing payloads and use `mulib_sshtransport_receive` for incoming
records and any reply frames it emits. When `mulib_sshtransport_rekey_due`
returns true, rotate keys promptly with `mulib_sshtransport_rekey` before
hitting a hard AEAD limit. During rekey a seal may be blocked; retry after local
`NEWKEYS` has been queued. The first exchange hash remains the session ID.
Strict key exchange is not advertised.

Authenticate the user before handling channels. On fatal error close the
connection, clear the transport and wipe private plaintext scratch.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sshtransport.h`](../kernels/mulib-sshtransport.h) |
| Implementation switch | `MULIB_IMPL_SSHTRANSPORT` |
| Repository-local includes | [mulib-sshkex](mulib-sshkex.md), [mulib-sshpacket](mulib-sshpacket.md) |
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
