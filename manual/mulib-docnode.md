# mulib-docnode

**Area:** Document IR  
**Implementation:** `MULIB_IMPL_DOCNODE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned ordered collection for document ir docnode rectangles and records; insertion, indexing, removal.

## API

```c
typedef struct mulib_docnode_item { double x,y,width,height; unsigned flags; } mulib_docnode_item_t;
typedef struct mulib_docnode_list { mulib_docnode_item_t *items; size_t count,capacity; } mulib_docnode_list_t;
int mulib_docnode_push(mulib_docnode_list_t *list, mulib_docnode_item_t item);
int mulib_docnode_remove(mulib_docnode_list_t *list, size_t index);
int mulib_docnode_bounds(const mulib_docnode_list_t *list, double *width, double *height);
void mulib_docnode_destroy(mulib_docnode_list_t *list);
```

## Behavior and limitations

Rectangle-based prototype data model; does not yet implement full advanced layout policy.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docnode.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-docnode.c -lm -o test
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
| Header | [`include/mulib-docnode.h`](../include/mulib-docnode.h) |
| Implementation switch | `MULIB_IMPL_DOCNODE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-docnode.c`](../examples/mulib-docnode.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-docnode.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
