# HTTP credentials

Define `MULIB_IMPL_AUTH`, `BASE64` and `SECURITY`. Bearer extraction borrows a
validated token; Basic decoding copies username:password with a terminating NUL,
rejecting missing colon/control/NUL bytes. No credential database or verification.
Require authenticated TLS externally. [Example](../examples/modules/auth.c),
[security boundary](storage-server.md#http-and-policies).
