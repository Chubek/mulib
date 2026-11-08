# MuIDL record generation

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MUIDL`, `DBSCHEMA`, `BUFFER` and the schema dependency closure.
Generates the small `record NAME { NAME: TYPE[?][ optional]; }` grammar defined
in the header, not a parser/compiler. Append is atomic, without a trailing NUL.
[Example](../examples/modules/muidl.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-muidl.h`](../include/mulib-muidl.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-muidl.h
 * @brief MuIDL flat-record schema source generation, not a general IDL parser.
 * Define MULIB_IMPL_MUIDL, DBSCHEMA, BUFFER and JSON dependencies.
 * Grammar: record NAME { NAME: TYPE[?][ optional]; ... }. Types are null,
 * bool, f64, string, json-array, json-object. ? means nullable. */
#ifndef MULIB_MUIDL_H
#define MULIB_MUIDL_H
#include "mulib-buffer.h"
#include "mulib-dbschema.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Atomic UTF-8 text append, no trailing NUL. 1 success, 0 invalid/OOM. */
  int mulib_muidl_generate (const mulib_dbschema_t *schema,
                            mulib_buffer_t *out);
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
| Header | [`include/mulib-muidl.h`](../include/mulib-muidl.h) |
| Implementation switch | `MULIB_IMPL_MUIDL` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-dbschema](mulib-dbschema.md) |
| Complete example | [`examples/modules/muidl.c`](../examples/modules/muidl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/muidl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
