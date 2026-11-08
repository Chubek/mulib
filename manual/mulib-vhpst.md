# Virtual hosts

[Manuals](README.md) · [Library index](index.md)

The ambiguous historical filename is interpreted as a virtual-host helper.
Define `MULIB_IMPL_VHPST` and `SECURITY` (`SEURITY` also works). Lookup validates
an ASCII DNS hostname and performs case-insensitive exact matching. The table
and contexts are borrowed. No wildcard/IDNA/port/IP-literal handling; SIZE_MAX
means no match or invalid. [Example](../examples/modules/vhpst.c),
[contracts](storage-server.md#http-and-policies).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vhpst.h`](../include/mulib-vhpst.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-vhpst.h
 * @brief Virtual-host selection over a borrowed exact DNS-name table.
 * The historical filename is interpreted as vhost. Define MULIB_IMPL_VHPST
 * and SECURITY. No wildcard, port, IP-literal or IDNA normalization. */
#ifndef MULIB_VHPST_H
#define MULIB_VHPST_H
#include "mulib-seurity.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vhpst
  {
    const char *name;
    void *context;
  } mulib_vhpst_t;
  /** Exact ASCII case-insensitive DNS host; SIZE_MAX for absent/invalid. */
  size_t mulib_vhpst_find (const mulib_vhpst_t *hosts, size_t count,
                           const char *host);
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
| Header | [`include/mulib-vhpst.h`](../include/mulib-vhpst.h) |
| Implementation switch | `MULIB_IMPL_VHPST` |
| Repository-local includes | [mulib-seurity](mulib-seurity.md) |
| Complete example | [`examples/modules/vhpst.c`](../examples/modules/vhpst.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vhpst.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
