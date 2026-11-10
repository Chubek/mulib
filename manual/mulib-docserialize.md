# mulib-docserialize

**Area:** Document IR  
**Implementation:** `MULIB_IMPL_DOCSERIALIZE` (define in exactly one C translation unit)  
**Priority:** P1  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Checked document ir docserialize text emission into an owned byte buffer.

## API

```c
int mulib_docserialize_write(mulib_typecore_buf_t *out, const char *source, size_t length);
int mulib_docserialize_write_cstr(mulib_typecore_buf_t *out, const char *source);
```

## Behavior and limitations

Text encoding uses json escaping. Full target format framing is a separate responsibility.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docserialize.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-docserialize.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Semantic tree serialization

With `MULIB_ENABLE_DOCIR`, `mulib_docserialize_tree` and `mulib_docserialize_read`
implement [MuDocIR binary version 1](docir-publishing.md#mudocir-binary-version-1).
Kinds, parents, UTF-8 text, safe IDs/targets, scaled-point styles and source spans
round-trip independently of host padding/endianness. Reads publish a fresh owned
tree only after complete validation; writes append the entire record atomically.
Caller limits bound nodes/depth/strings. Legacy string-escaping APIs are unchanged.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-docserialize.h`](../include/mulib-docserialize.h) |
| Implementation switch | `MULIB_IMPL_DOCSERIALIZE` |
| Repository-local includes | [`include/detail/mulib-docserialize-tree.h`](../include/detail/mulib-docserialize-tree.h), [mulib-docir](mulib-docir.md), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-docserialize.c`](../examples/mulib-docserialize.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-docserialize.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
