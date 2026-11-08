# Database value deserialization

[Manuals](README.md) · [Library index](index.md)

Historical filename retained; API spelling is `mulib_dbdeserializer_read`.
Define `MULIB_IMPL_DBDESIALIZER` and the complete [dbserializer](mulib-dbserializer.md)
closure. Returns an owned JSON value or NULL. Input must contain exactly one
supported value and is limited to 16 MiB. No automatic format detection.
[Example](../examples/modules/dbdesializer.c), [profiles](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbdesializer.h`](../include/mulib-dbdesializer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbdesializer.h
 * @brief Exact owned value decoding, retaining the historical filename.
 * Define MULIB_IMPL_DBDESIALIZER and all codec dependencies.
 * JSON input is bounded to 16 MiB like the binary codecs. NULL means failure.
 */
#ifndef MULIB_DBDESIALIZER_H
#define MULIB_DBDESIALIZER_H
#include "mulib-dbserializer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_json_t *mulib_dbdeserializer_read (mulib_dbformat format,
                                           const void *data, size_t length);
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
| Header | [`include/mulib-dbdesializer.h`](../include/mulib-dbdesializer.h) |
| Implementation switch | `MULIB_IMPL_DBDESIALIZER` |
| Repository-local includes | [mulib-dbserializer](mulib-dbserializer.md) |
| Complete example | [`examples/modules/dbdesializer.c`](../examples/modules/dbdesializer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbdesializer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
