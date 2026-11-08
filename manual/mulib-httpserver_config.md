# HTTP server limits

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HTTPSERVER_CONFIG`. The HTTP model is a declaration dependency.
Defaults specify header/body/buffer/connection limits and idle timeout. Validation
requires explicit limits and checks sum overflow. No listener, resource allocation
or timeout enforcement. [Example](../examples/modules/httpserver_config.c),
[contracts](storage-server.md#http-and-policies).

Header-array allocation must be representable (`max_headers * sizeof(header)`),
as must the aggregate buffer budget (`max_connections * max_buffer_bytes`).
Validation rejects zero connections/timeouts/limits and per-message EOF or
response-context flags. Checked arithmetic prevents wraparound in these budgets.

The buffer lower bound is headers plus **decoded** body bytes. Chunk framing,
buffered pipeline data and application metadata can require additional room;
choose max_buffer_bytes for the intended buffering/streaming policy. Validation
does not reserve memory or guarantee that every maximally chunked representation
fits a buffer at that lower bound. Actual allocation and connection admission
remain the server's responsibility.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-httpserver_config.h`](../include/mulib-httpserver_config.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-httpserver_config.h
 * @brief Transport-independent HTTP server resource configuration.
 * Define MULIB_IMPL_HTTPSERVER_CONFIG. No listener creation. */
#ifndef MULIB_HTTPSERVER_CONFIG_H
#define MULIB_HTTPSERVER_CONFIG_H
#include "mulib-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_httpserver_config
  {
    mulib_http_options_t limits;
    size_t max_buffer_bytes, max_connections;
    unsigned idle_timeout_ms;
  } mulib_httpserver_config_t;
  /** Return explicit HTTP, buffer, connection and idle-time limits. */
  mulib_httpserver_config_t mulib_httpserver_config_default (void);
  /** Validate explicit nonzero limits, representable header storage and total
   * per-connection buffer budget. The buffer must fit headers plus decoded body;
   * chunk framing and pipelined data may require additional space. Flags must be
   * zero (EOF/response context are per-message, not server-wide). This function
   * does not allocate resources or enforce deadlines/connection limits. */
  int
  mulib_httpserver_config_validate (const mulib_httpserver_config_t *config);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-httpserver_config.h`](../include/mulib-httpserver_config.h) |
| Implementation switch | `MULIB_IMPL_HTTPSERVER_CONFIG` |
| Repository-local includes | [mulib-http](mulib-http.md) |
| Complete example | [`examples/modules/httpserver_config.c`](../examples/modules/httpserver_config.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/httpserver_config.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
