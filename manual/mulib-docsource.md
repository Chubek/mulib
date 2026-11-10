# mulib-docsource

**Area:** Document IR  
**Implementation:** `MULIB_IMPL_DOCSOURCE` (define in exactly one C translation unit)  
**Priority:** P1  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned bounded registry of source-origin mappings; replace, lookup and remove by name.

## API

```c
typedef struct mulib_docsource_entry { char *key; char *value; } mulib_docsource_entry_t;
typedef struct mulib_docsource_table { mulib_docsource_entry_t *entries; size_t count, capacity; } mulib_docsource_table_t;
void mulib_docsource_destroy(mulib_docsource_table_t *table);
int mulib_docsource_set(mulib_docsource_table_t *table, const char *key, const char *value);
const char *mulib_docsource_get(const mulib_docsource_table_t *table, const char *key);
int mulib_docsource_erase(mulib_docsource_table_t *table, const char *key);
```

## Behavior and limitations

This is a usable registry substrate, not a complete parser, formatter or standards implementation.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docsource.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-docsource.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Typed source spans

With `MULIB_ENABLE_DOCIR`, `mulib_docsource_bind(node,source,file,begin,end)` checks
a byte span against a typesrc source (at most 64 MiB), computes its 1-based byte
line/column and copies provenance into the managed node. Invalid spans preserve
the old annotation. Snapshots and MuDocIR v1 retain it without external file I/O.
The original string registry remains available; see [the pipeline contract](docir-publishing.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-docsource.h`](../include/mulib-docsource.h) |
| Implementation switch | `MULIB_IMPL_DOCSOURCE` |
| Repository-local includes | [mulib-docir](mulib-docir.md), [mulib-typecore](mulib-typecore.md), [mulib-typesrc](mulib-typesrc.md) |
| Complete example | [`examples/mulib-docsource.c`](../examples/mulib-docsource.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-docsource.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
