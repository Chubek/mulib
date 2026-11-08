# Owned HTTP responses

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HTTP_RESPONSE`, `WEBFRAMEWORK` and its HTTP closure. Reuses
the existing bounded validated reply builder, including its status/body rules.
Inputs are borrowed; output owns memory only on OK. Free with
`mulib_http_message_free`. [Example](../examples/modules/http_response.c),
[contracts](storage-server.md#http-and-policies).

Status must be 100..599. Bodies are rejected for 1xx, 204, 205 and 304; a 205
response is emitted with explicit zero-length framing. Content-Type is optional
and validated against header injection. Nonempty body data must have a valid
pointer and is copied byte-for-byte, including embedded NUL. The helper supplies
a reason phrase (a generic phrase for statuses without a dedicated mapping).
The result must be empty/unowned before success and freed before reuse.

This convenience constructor does not know whether the request was HEAD or
CONNECT. For those contexts, custom headers/reason phrases or nondefault limits,
use the shared HTTP message and serializer with the appropriate response flags.
Failure—including invalid status/body, oversized output and allocation failure—
leaves the output unchanged. The HTTP hardening and allocation suites verify this.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-http_response.h`](../include/mulib-http_response.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-http_response.h
 * @brief Owned response construction over the existing webframework helper.
 * Define MULIB_IMPL_HTTP_RESPONSE and WEBFRAMEWORK/HTTP dependencies. */
#ifndef MULIB_HTTP_RESPONSE_H
#define MULIB_HTTP_RESPONSE_H
#include "mulib-webframework.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Construct an independently owned response under default HTTP limits.
   * status must be 100..599; 1xx/204/205/304 must not carry content. Content-Type
   * is optional and validated against header injection. A nonempty body needs
   * a non-NULL pointer and is copied byte-for-byte. Failure leaves out unchanged;
   * successful output must be freed with message_free before reuse. The helper
   * supplies a reason phrase and canonical framing, not request HEAD/CONNECT
   * context; use the underlying HTTP message serializer for contextual replies. */
  mulib_http_result mulib_http_response_create (mulib_http_message_t *out,
                                                unsigned status,
                                                const char *content_type,
                                                const void *body,
                                                size_t length);
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
| Header | [`include/mulib-http_response.h`](../include/mulib-http_response.h) |
| Implementation switch | `MULIB_IMPL_HTTP_RESPONSE` |
| Repository-local includes | [mulib-webframework](mulib-webframework.md) |
| Complete example | [`examples/modules/http_response.c`](../examples/modules/http_response.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/http_response.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
