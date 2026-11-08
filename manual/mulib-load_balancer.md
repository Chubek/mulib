# Load balancing

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_LOAD_BALANCER` and `UPSTREAM`. Round-robin or least-active
selection over a borrowed array, using round-robin tie breaking. Returns index
or SIZE_MAX. Selection does not acquire; caller synchronizes and balances
acquire/release. [Example](../examples/modules/load_balancer.c),
[contracts](storage-server.md#http-and-policies).

## Selection contract

The upstream array is borrowed for the call. Provide a persistent `size_t`
cursor, initially zero. Round-robin selection starts at `cursor % count` and
chooses the first [available upstream](mulib-upstream.md). With nonzero
`least_connections`, it scans all available entries for the smallest active
count; ties use that same rotating order.

Return is the selected array index, or `SIZE_MAX` for no available entry or
invalid arguments. On success the cursor advances past the selected index;
otherwise it is unchanged. Selection does **not** increment the active count.
Acquire the selected upstream and later release it; synchronize selection and
acquisition together when sharing state. Selection is O(count), allocation-free,
and performs no network I/O.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-load_balancer.h`](../include/mulib-load_balancer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-load_balancer.h
 * @brief Round-robin and least-connections selection over borrowed upstreams.
 * Select does not acquire; caller owns synchronization. SIZE_MAX means none.
 * Define MULIB_IMPL_LOAD_BALANCER and UPSTREAM. */
#ifndef MULIB_LOAD_BALANCER_H
#define MULIB_LOAD_BALANCER_H
#include "mulib-upstream.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_load_balancer_select (const mulib_upstream_t *items,
                                     size_t count, size_t *cursor,
                                     int least_connections);
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
| Header | [`include/mulib-load_balancer.h`](../include/mulib-load_balancer.h) |
| Implementation switch | `MULIB_IMPL_LOAD_BALANCER` |
| Repository-local includes | [mulib-upstream](mulib-upstream.md) |
| Complete example | [`examples/modules/load_balancer.c`](../examples/modules/load_balancer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/load_balancer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
