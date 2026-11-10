# mulib-glossary

**Area:** References  
**Implementation:** `MULIB_IMPL_GLOSSARY` (define in exactly one C translation unit)  
**Priority:** P2  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned bounded registry of glossary terms and definitions; replace, lookup and remove by name.

## API

```c
typedef struct mulib_glossary_entry { char *key; char *value; } mulib_glossary_entry_t;
typedef struct mulib_glossary_table { mulib_glossary_entry_t *entries; size_t count, capacity; } mulib_glossary_table_t;
void mulib_glossary_destroy(mulib_glossary_table_t *table);
int mulib_glossary_set(mulib_glossary_table_t *table, const char *key, const char *value);
const char *mulib_glossary_get(const mulib_glossary_table_t *table, const char *key);
int mulib_glossary_erase(mulib_glossary_table_t *table, const char *key);
```

## Behavior and limitations

This is a usable registry substrate, not a complete parser, formatter or standards implementation.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-glossary.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-glossary.c -lm -o test
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
| Header | [`include/mulib-glossary.h`](../include/mulib-glossary.h) |
| Implementation switch | `MULIB_IMPL_GLOSSARY` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-glossary.c`](../examples/mulib-glossary.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-glossary.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
