# Flat record schema

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_DBSCHEMA`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`. Schemas and
field names are borrowed ASCII identifiers. Validation rejects duplicate fields;
record checking rejects unknown/missing-required/wrong-type members. Nullable
and required are independent. Nested values have no recursive constraints.
[Example](../examples/modules/dbschema.c), [contracts](storage-server.md).

## Schema and validation contract

Schemas borrow their name, field array, and field names. Keep those alive for
every validation call; there is no constructor, copy, or destructor. Identifiers
must match ASCII `[A-Za-z_][A-Za-z0-9_]*`. Validate rejects duplicate field names,
invalid JSON type values, and required/nullable flags other than exactly 0 or 1.
An empty field list is valid.

Check requires a valid schema and a JSON object. Required controls whether a
field may be absent; nullable additionally permits an explicit JSON null even
when the declared type differs. A present value otherwise must have the exact
declared type. Unknown keys are rejected; nested array/object contents are not
recursively constrained. All three functions return Boolean 1/0, allocate no
state, and do not mutate either schema or record. This is flat record checking,
not JSON Schema or type coercion.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbschema.h`](../include/mulib-dbschema.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbschema.h
 * @brief Borrowed flat record schemas over the shared JSON value model.
 * Define MULIB_IMPL_DBSCHEMA and JSON dependencies. Exact JSON types, no
 * coercion; nullable permits explicit null, required controls absent fields. */
#ifndef MULIB_DBSCHEMA_H
#define MULIB_DBSCHEMA_H
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dbschema_field
  {
    const char *name;
    mulib_json_type type;
    int required, nullable;
  } mulib_dbschema_field_t;
  typedef struct mulib_dbschema
  {
    const char *name;
    const mulib_dbschema_field_t *fields;
    size_t count;
  } mulib_dbschema_t;
  int mulib_dbschema_identifier (const char *name);
  int mulib_dbschema_validate (const mulib_dbschema_t *schema);
  /** Reject unknown fields, missing required fields and wrong types. Nested
   * object/array contents are not constrained. */
  int mulib_dbschema_check (const mulib_dbschema_t *schema,
                            const mulib_json_t *record);
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
| Header | [`include/mulib-dbschema.h`](../include/mulib-dbschema.h) |
| Implementation switch | `MULIB_IMPL_DBSCHEMA` |
| Repository-local includes | [mulib-json](mulib-json.md) |
| Complete example | [`examples/modules/dbschema.c`](../examples/modules/dbschema.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbschema.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
