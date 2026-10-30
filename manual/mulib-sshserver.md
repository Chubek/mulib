# sshserver — SSH user-authentication payloads

Define `MULIB_IMPL_ENDIAN` and `MULIB_IMPL_SSHSERVER`.
See [`examples/modules/sshserver.c`](../examples/modules/sshserver.c).

`auth_parse` consumes an entire SSH_MSG_USERAUTH_REQUEST payload (message 50),
requiring service `ssh-connection`, and supports methods `none` and `password`
without password-change requests. SSH strings are uint32-length-prefixed and
binary-safe. Username must be 1..1024 bytes; password is at most 4096 bytes.
The result borrows both strings. Return 1 success, 0 malformed, -1 unsupported
method/service/password change. Outputs stay unchanged on failure.

Initialize attempt policy with a positive maximum. `authenticate` consumes an
attempt for every submitted request, including malformed or unsupported input.
`none` always fails. A supported password request invokes a caller verifier
which returns 1 to accept. Success becomes terminal; exhausted or already
authenticated policy returns -1, rejection returns 0, acceptance returns 1.
No credentials are retained. `auth_reply` encodes message 52 success or message
51 failure with `password` as the available method and partial-success=false.

These are payload and policy helpers, not an SSH daemon. The application must
obtain payloads from a verified encrypted SSH transport and supply account
authorization. Public-key authentication, key exchange, channels and command
execution require a separate backend. Tests: `tests/test_protocol_extensions.c`.
