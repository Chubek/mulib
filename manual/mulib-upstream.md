# Upstream endpoints

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_UPSTREAM`. Host names are borrowed, ports validated on
availability checks. Healthy endpoints with free capacity can be acquired;
release refuses underflow. Zero max_active means unlimited. No sockets/DNS.
[Example](../examples/modules/upstream.c), [contracts](storage-server.md).

## Endpoint lifetime and accounting

Fill a caller-owned `mulib_upstream_t` with a borrowed nonempty host string,
port 1..65535, health flag, and active/max-active counters. Keep host storage
alive while the endpoint is used. A zero maximum means unlimited, subject to
the `SIZE_MAX` active-count bound. Zero-initialized health is unavailable until
the application or health checker marks it healthy.

Available returns 1 only for a valid, healthy endpoint below its active limit.
Acquire checks that predicate and increments active on success (1); failure (0)
leaves it unchanged. Release decrements any nonzero active count, regardless of
current health; NULL or zero-active state returns 0. Pair each successful
acquisition with one release. There is no allocation, socket connection, DNS
lookup, or internal locking.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-upstream.h`](../include/mulib-upstream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-upstream.h
 * @brief Borrowed upstream endpoints and bounded in-flight accounting.
 * No sockets or DNS. Define MULIB_IMPL_UPSTREAM. */
#ifndef MULIB_UPSTREAM_H
#define MULIB_UPSTREAM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_upstream
  {
    const char *host;
    unsigned port;
    size_t active, max_active; /* zero maximum is unlimited */
    int healthy;
  } mulib_upstream_t;
  int mulib_upstream_available (const mulib_upstream_t *upstream);
  int mulib_upstream_acquire (mulib_upstream_t *upstream);
  int mulib_upstream_release (mulib_upstream_t *upstream);
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
| Header | [`include/mulib-upstream.h`](../include/mulib-upstream.h) |
| Implementation switch | `MULIB_IMPL_UPSTREAM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/upstream.c`](../examples/modules/upstream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/upstream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
