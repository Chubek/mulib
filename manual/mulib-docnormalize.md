# mulib-docnormalize

**Area:** Document IR  
**Implementation:** `MULIB_IMPL_DOCNORMALIZE` (define in exactly one C translation unit)  
**Priority:** P1  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Checked document ir docnormalize text emission into an owned byte buffer.

## API

```c
int mulib_docnormalize_write(mulib_typecore_buf_t *out, const char *source, size_t length);
int mulib_docnormalize_write_cstr(mulib_typecore_buf_t *out, const char *source);
```

## Behavior and limitations

Text encoding uses raw escaping. Full target format framing is a separate responsibility.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docnormalize.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-docnormalize.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Owned tree normalization

`mulib_docnormalize_tree` (with `MULIB_ENABLE_DOCIR`) returns an independent tree
or NULL without changing its input. It merges compatible adjacent text siblings
without IDs/targets/source annotations; differing styles/provenance prevent a
merge. Whitespace and original Unicode are preserved: this is **not** Unicode
normalization or arbitrary document rewriting. See [the pipeline contract](docir-publishing.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-docnormalize.h`](../include/mulib-docnormalize.h) |
| Implementation switch | `MULIB_IMPL_DOCNORMALIZE` |
| Repository-local includes | [mulib-docir](mulib-docir.md), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-docnormalize.c`](../examples/mulib-docnormalize.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-docnormalize.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
