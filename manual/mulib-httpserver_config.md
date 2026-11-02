# HTTP server limits

Define `MULIB_IMPL_HTTPSERVER_CONFIG`. The HTTP model is a declaration dependency.
Defaults specify header/body/buffer/connection limits and idle timeout. Validation
requires explicit limits and checks sum overflow. No listener, resource allocation
or timeout enforcement. [Example](../examples/modules/httpserver_config.c),
[contracts](storage-server.md#http-and-policies).
