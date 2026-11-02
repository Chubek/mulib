# OAuth2 policy adapter

Define `MULIB_IMPL_OAUTH2`, `AUTH`, `BASE64` and `SECURITY`. Authorization
requires an application verifier which checks token validity, issuer, audience,
expiry and scope; missing policy denies. State comparison requires nonempty
already-decoded strings, not nonce creation/storage/consumption. No JWT/server/
network client. [Example](../examples/modules/oauth2.c),
[security boundary](storage-server.md#http-and-policies).
