# Request policy

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_WAF`, `SECURITY` and the [HTTP](mulib-http.md) dependency
closure. On an already parsed request, checks size limits, an exact method
allowlist and strict origin path. Query/body attack signatures are not inspected;
this is not a comprehensive WAF. Invalid policy denies. [Example](../examples/modules/waf.c),
[security boundary](storage-server.md#http-and-policies).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-waf.h`](../include/mulib-waf.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-waf.h
 * @brief Fail-closed request size/method/path policy, not a signature WAF.
 * Define MULIB_IMPL_WAF and SECURITY. Parsed HTTP model is borrowed. */
#ifndef MULIB_WAF_H
#define MULIB_WAF_H
#include "mulib-http.h"
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_waf
  {
    size_t max_target, max_body;
    const char *const *methods;
    size_t method_count;
  } mulib_waf_t;
  /** Only strict origin paths with optional query; methods are exact allowlist.
   * Returns 1 allowed, 0 denied/invalid. Does not inspect query/body
   * signatures. */
  int mulib_waf_check (const mulib_waf_t *policy,
                       const mulib_http_message_t *request);
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
| Header | [`include/mulib-waf.h`](../include/mulib-waf.h) |
| Implementation switch | `MULIB_IMPL_WAF` |
| Repository-local includes | [mulib-http](mulib-http.md), [mulib-seurity](mulib-seurity.md) |
| Complete example | [`examples/modules/waf.c`](../examples/modules/waf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/waf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
