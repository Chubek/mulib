# Database value serialization

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_DBSERIALIZER`, `BSON`, `CBOR`, `MESSAGEPACK`, `BUFFER`, `JSON`,
`PARSEC`, `STRBUF` and `UTF8`. Dispatch selects one of four formats; all linked
codec implementations are required. Writes atomically append and return 1/0.
Profiles are intentionally restricted to shared JSON values.
[Example](../examples/modules/dbserializer.c), [profiles](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbserializer.h`](../include/mulib-dbserializer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbserializer.h
 * @brief Explicit JSON/BSON/MessagePack/CBOR serialization of shared values.
 * Define MULIB_IMPL_DBSERIALIZER plus selected codec dependencies. All codecs
 * are linked by this dispatcher; output append is failure-atomic. */
#ifndef MULIB_DBSERIALIZER_H
#define MULIB_DBSERIALIZER_H
#include "mulib-bson.h"
#include "mulib-cbor.h"
#include "mulib-messagepack.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_dbformat
  {
    MULIB_DBFORMAT_JSON,
    MULIB_DBFORMAT_BSON,
    MULIB_DBFORMAT_MESSAGEPACK,
    MULIB_DBFORMAT_CBOR
  } mulib_dbformat;
  int mulib_dbserializer_write (mulib_dbformat format,
                                const mulib_json_t *value, mulib_buffer_t *out);
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
| Header | [`include/mulib-dbserializer.h`](../include/mulib-dbserializer.h) |
| Implementation switch | `MULIB_IMPL_DBSERIALIZER` |
| Repository-local includes | [mulib-bson](mulib-bson.md), [mulib-cbor](mulib-cbor.md), [mulib-messagepack](mulib-messagepack.md) |
| Complete example | [`examples/modules/dbserializer.c`](../examples/modules/dbserializer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbserializer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
