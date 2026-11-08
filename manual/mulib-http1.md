# HTTP/1.1

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HTTP1` and the [HTTP](mulib-http.md) closure. The entry point
forwards to the existing strict parser, preserving limits, ownership, MORE/EOF
handling and framing rules. It is not a second parser or socket implementation.
[Example](../examples/modules/http1.c), [contracts](storage-server.md).

## Serialization and ownership

`mulib_http1_serialize(message, options, &bytes, &length)` is the symmetric
HTTP/1.1 serialization entry point. It uses exactly `mulib_http_serialize`'s
validation, aggregate resource limits, binary handling and response-context
rules. On success bytes is malloc-owned and has an extra NUL outside length;
free it with `free`. On failure both output arguments remain unchanged.

`mulib_http1_parse` transfers an owned message only on OK; failure leaves the
message unchanged and resets an optional consumed output to zero. Do not reuse
an owned message without freeing it first. HEAD/CONNECT context flags are
response-only and mutually exclusive. See [HTTP](mulib-http.md) for framing,
status-205 handling, EOF and pipeline boundaries.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-http1.h`](../include/mulib-http1.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-http1.h
 * @brief Explicit HTTP/1.1 entry point reusing strict shared framing.
 * Define MULIB_IMPL_HTTP1 and HTTP dependencies. */
#ifndef MULIB_HTTP1_H
#define MULIB_HTTP1_H
#include "mulib-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Parse one HTTP/1.1 message with the exact HTTP ownership, framing and
   * limit contract. On failure out is unchanged and consumed (if given) is 0.
   * Response context flags are mutually exclusive and invalid for requests. */
  mulib_http_result mulib_http1_parse (const void *data, size_t length,
                                       int response,
                                       const mulib_http_options_t *options,
                                       mulib_http_message_t *out,
                                        size_t *consumed);
  /** Serialize validated HTTP/1.1; identical to mulib_http_serialize.
   * On OK transfers malloc-owned bytes with an extra NUL outside length.
   * On failure both output arguments remain unchanged. */
  mulib_http_result mulib_http1_serialize (const mulib_http_message_t *,
                                          const mulib_http_options_t *,
                                          unsigned char **bytes, size_t *length);
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
| Header | [`include/mulib-http1.h`](../include/mulib-http1.h) |
| Implementation switch | `MULIB_IMPL_HTTP1` |
| Repository-local includes | [mulib-http](mulib-http.md) |
| Complete example | [`examples/modules/http1.c`](../examples/modules/http1.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/http1.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
