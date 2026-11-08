# WIT record generation

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_WIT`, `DBSCHEMA`, `BUFFER` and the schema dependency closure.
Only bool/f64/string fields and lowercase underscore-separated identifiers are
supported. Names become escaped kebab-case. Optional/nullable each add an option
layer. Append is atomic without a NUL. No parser/component runtime/bindings.
[Example](../examples/modules/wit.c), [mapping](storage-server.md#idl-and-native-output).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-wit.h`](../include/mulib-wit.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-wit.h
 * @brief Flat primitive schemas to WIT record source, not component bindings.
 * Define MULIB_IMPL_WIT, DBSCHEMA, BUFFER and JSON dependencies.
 * bool/f64/string only. Lowercase identifier words separated by underscores
 * become kebab-case; all names use % escaping. Missing and nullable each add
 * one option layer, preserving both distinctions when both are permitted. */
#ifndef MULIB_WIT_H
#define MULIB_WIT_H
#include "mulib-buffer.h"
#include "mulib-dbschema.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_wit_generate (const mulib_dbschema_t *schema, mulib_buffer_t *out);
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
| Header | [`include/mulib-wit.h`](../include/mulib-wit.h) |
| Implementation switch | `MULIB_IMPL_WIT` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-dbschema](mulib-dbschema.md) |
| Complete example | [`examples/modules/wit.c`](../examples/modules/wit.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wit.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
