# sshauth — SSH user authentication

Use these payloads over an authenticated and encrypted
[transport](mulib-sshtransport.md). Negotiate `ssh-userauth`, then exchange
requests for `ssh-connection`. A public-key query only asks whether the key is
acceptable. A signed request must independently verify against the session ID
and pass the application's authorization check for the named account. Password
requests require an application verifier; `none` does not authenticate.
The [connection driver](mulib-sshconnection.md) enforces sequencing and an
attempt limit. Authorize each signed request independently of earlier queries,
including if the account or key changes between requests.

Parser slices borrow the input. Do not retain passwords or pointers into
reusable plaintext buffers; wipe those buffers after use. Do not dispatch
[channels](mulib-sshchannel.md) before successful authentication.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sshauth.h`](../include/mulib-sshauth.h) |
| Implementation switch | `MULIB_IMPL_SSHAUTH` |
| Repository-local includes | [mulib-ed25519](mulib-ed25519.md), [mulib-sshkey](mulib-sshkey.md), [mulib-sshproto](mulib-sshproto.md) |
| Complete example | [`examples/ssh/kernels.c`](../examples/ssh/kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/ssh/kernels.c -pthread -lm -ldl -lcrypto -o mulib-example
```
<!-- /manual-reference -->
