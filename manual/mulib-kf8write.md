# mulib-kf8write

**Area:** Output  
**Implementation:** `MULIB_IMPL_KF8WRITE` (define in exactly one C translation unit)  
**Priority:** P1  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

KF8 FDST flow table construction (building block, not an AZW3 ebook writer).

## API

```c
typedef struct mulib_kf8write_flow { uint32_t offset,length; } mulib_kf8write_flow_t;
int mulib_kf8write_fdst(const mulib_kf8write_flow_t *flows,size_t count,mulib_typecore_buf_t *out);
```

## Behavior and limitations

Only FDST record construction. Standalone KF8/AZW3 packages need multiple additional record/index encoders.

The record length `12 + 8 * count` must fit both `size_t` and the unsigned 32-bit
FDST length field. Oversized counts are rejected before reading the flow array.
Flows must be ordered, nonoverlapping, and end at or before `UINT32_MAX`.
An empty list is accepted (a null flow pointer is allowed only for zero count).
On success the complete record is appended to the caller's buffer; on failure
its existing bytes and size are unchanged. The caller frees the buffer with
`mulib_typecore_free`. No named KF8 conformance profile is claimed.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-kf8write.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-kf8write.c -lm -o test
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
| Header | [`include/mulib-kf8write.h`](../include/mulib-kf8write.h) |
| Implementation switch | `MULIB_IMPL_KF8WRITE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-kf8write.c`](../examples/mulib-kf8write.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-kf8write.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
