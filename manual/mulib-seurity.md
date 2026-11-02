# Security byte helpers

Historical filename and `MULIB_IMPL_SEURITY` remain supported; the canonical
switch is `MULIB_IMPL_SECURITY`. No dependencies. Token checks, ASCII equality,
byte comparison without early exit and strict path decoding are provided.
Not cryptography or a compiler-independent constant-time guarantee. Path failure
leaves output unchanged; see [path policy](storage-server.md#http-and-policies)
and [example](../examples/modules/seurity.c).
