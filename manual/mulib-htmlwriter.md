# mulib-htmlwriter

**Area:** Output  
**Implementation:** `MULIB_IMPL_HTMLWRITER` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Self-contained semantic HTML5 document writer.

## API

```c
int mulib_htmlwriter_document(const char *title,const char *body,mulib_typecore_buf_t *out);
```

## Behavior and limitations

Appends a fixed English-language, one-paragraph HTML document to the
caller-owned `out` buffer. `title` and `body` are borrowed, NUL-terminated
UTF-8 plain-text strings; empty strings are accepted. Their bytes are copied,
escaping markup-sensitive characters. Malformed UTF-8 (including truncated
sequences, overlong encodings, surrogate values and values above U+10FFFF)
is rejected rather than copied into a document labeled UTF-8.

Initialize `out` to zero or pass an existing initialized typecore buffer.
The function returns 1 on success and 0 on invalid arguments, malformed UTF-8,
size overflow or allocation failure. Failure leaves `out` unchanged. No NUL
terminator is appended; use `mulib_typecore_ends` when a C string is needed,
and release the buffer with `mulib_typecore_free`.

Embedded NUL, raw markup, document-tree and CSS integration are outside this
API. UTF-8 validation does not enforce HTML-specific restrictions on Unicode
controls or noncharacters, and HTML parsers may normalize literal line endings.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Ikernels examples/mulib-htmlwriter.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Ikernels tests/test-htmlwriter.c -lm -o test
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
| Header | [`kernels/mulib-htmlwriter.h`](../kernels/mulib-htmlwriter.h) |
| Implementation switch | `MULIB_IMPL_HTMLWRITER` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-htmlwriter.c`](../examples/mulib-htmlwriter.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/mulib-htmlwriter.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
