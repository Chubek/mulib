# mulib-docstyle

**Area:** Document IR  
**Implementation:** `MULIB_IMPL_DOCSTYLE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned bounded registry of document style properties; replace, lookup and remove by name.

## API

```c
typedef struct mulib_docstyle_entry { char *key; char *value; } mulib_docstyle_entry_t;
typedef struct mulib_docstyle_table { mulib_docstyle_entry_t *entries; size_t count, capacity; } mulib_docstyle_table_t;
void mulib_docstyle_destroy(mulib_docstyle_table_t *table);
int mulib_docstyle_set(mulib_docstyle_table_t *table, const char *key, const char *value);
const char *mulib_docstyle_get(const mulib_docstyle_table_t *table, const char *key);
int mulib_docstyle_erase(mulib_docstyle_table_t *table, const char *key);
```

## Behavior and limitations

This is a usable registry substrate, not a complete parser, formatter or standards implementation.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docstyle.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-docstyle.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-docstyle.h`](../include/mulib-docstyle.h) |
| Implementation switch | `MULIB_IMPL_DOCSTYLE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-docstyle.c`](../examples/mulib-docstyle.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-docstyle.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
