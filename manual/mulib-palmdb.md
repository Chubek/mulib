# mulib-palmdb

**Area:** Output  
**Implementation:** `MULIB_IMPL_PALMDB` (define in exactly one C translation unit)  
**Priority:** P1  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Palm database container writer for fixed records.

## API

```c
typedef struct mulib_palmdb_record { const void *data;size_t size; } mulib_palmdb_record_t;
int mulib_palmdb_write(const char *name,const char type[4],const char creator[4],const mulib_palmdb_record_t *records,size_t count,mulib_typecore_buf_t *out);
```

## Behavior and limitations

Uncompressed PDB records and correct record offsets. Does not implement MOBI metadata or KF8 indexes.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-palmdb.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-palmdb.c -lm -o test
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
| Header | [`include/mulib-palmdb.h`](../include/mulib-palmdb.h) |
| Implementation switch | `MULIB_IMPL_PALMDB` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-palmdb.c`](../examples/mulib-palmdb.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-palmdb.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
