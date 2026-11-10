# mulib-typesrc

**Area:** Language  
**Implementation:** `MULIB_IMPL_TYPESRC` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned source text with location-aware line and column lookup.

## API

```c
typedef struct mulib_typesrc_source { char *data; size_t size; } mulib_typesrc_source_t;
int mulib_typesrc_init(mulib_typesrc_source_t *s,const char *bytes,size_t size);
int mulib_typesrc_locate(const mulib_typesrc_source_t *s,size_t offset,size_t *line,size_t *column);
void mulib_typesrc_destroy(mulib_typesrc_source_t *s);
```

## Behavior and limitations

Baseline bounded functionality; this is not a complete application-level engine.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typesrc.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-typesrc.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Source lifecycle

Use `init` for fresh source storage and `destroy` before reinitialization. New
`mulib_typesrc_assign` transactionally replaces an initialized/zero source,
including self-aliased input, without leaking the prior allocation. Configure
paired `MULIB_TYPESRC_MALLOC`/`MULIB_TYPESRC_FREE` hooks in its implementation TU.
Location lookup counts bytes (not Unicode scalars or display columns) and rejects
a null data pointer with nonzero size without modifying outputs.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-typesrc.h`](../include/mulib-typesrc.h) |
| Implementation switch | `MULIB_IMPL_TYPESRC` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-typesrc.c`](../examples/mulib-typesrc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-typesrc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
