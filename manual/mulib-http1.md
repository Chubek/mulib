# HTTP/1.1

Define `MULIB_IMPL_HTTP1` and the [HTTP](mulib-http.md) closure. The entry point
forwards to the existing strict parser, preserving limits, ownership, MORE/EOF
handling and framing rules. It is not a second parser or socket implementation.
[Example](../examples/modules/http1.c), [contracts](storage-server.md).
