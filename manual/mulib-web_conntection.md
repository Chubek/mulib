# Incremental HTTP connection core

[Manuals](README.md) · [Library index](index.md)

Historical filename retained; define `MULIB_IMPL_WEB_CONNECTION`, `HTTP1` and
the HTTP closure. Zero-initialize, set byte limit/options, feed bounded chunks,
then next parses/removes one message. EOF is permanent; pipelined data remains.
No sockets, TLS or timeout enforcement. [Example](../examples/modules/web_conntection.c),
[contracts](storage-server.md#http-and-policies).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-web_conntection.h`](../include/mulib-web_conntection.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-web_conntection.h
 * @brief Bounded incremental HTTP/1 receive buffering (historical spelling).
 * Define MULIB_IMPL_WEB_CONNECTION, BUFFER and HTTP dependencies.
 * Zero initialize; configure limit and options before feed. No sockets/TLS. */
#ifndef MULIB_WEB_CONNTECTION_H
#define MULIB_WEB_CONNTECTION_H
#include "mulib-http1.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_web_connection
  {
    mulib_buffer_t pending;
    mulib_http_options_t options;
    size_t limit;
    int response, eof;
  } mulib_web_connection_t;
  mulib_http_result
  mulib_web_connection_feed (mulib_web_connection_t *connection,
                             const void *data, size_t length, int eof);
  /** Parse/remove exactly one message; pipelined remainder is retained. */
  mulib_http_result
  mulib_web_connection_next (mulib_web_connection_t *connection,
                             mulib_http_message_t *out);
  void mulib_web_connection_free (mulib_web_connection_t *connection);
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
| Header | [`include/mulib-web_conntection.h`](../include/mulib-web_conntection.h) |
| Implementation switch | `MULIB_IMPL_WEB_CONNECTION` |
| Repository-local includes | [mulib-http1](mulib-http1.md) |
| Complete example | [`examples/modules/web_conntection.c`](../examples/modules/web_conntection.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/web_conntection.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
