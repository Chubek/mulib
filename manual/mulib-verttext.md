# mulib-verttext

**Area:** References  
**Implementation:** `MULIB_IMPL_VERTTEXT` (define in exactly one C translation unit)  
**Priority:** P2  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned ordered collection for references verttext rectangles and records; insertion, indexing, removal.

## API

```c
typedef struct mulib_verttext_item { double x,y,width,height; unsigned flags; } mulib_verttext_item_t;
typedef struct mulib_verttext_list { mulib_verttext_item_t *items; size_t count,capacity; } mulib_verttext_list_t;
int mulib_verttext_push(mulib_verttext_list_t *list, mulib_verttext_item_t item);
int mulib_verttext_remove(mulib_verttext_list_t *list, size_t index);
int mulib_verttext_bounds(const mulib_verttext_list_t *list, double *width, double *height);
void mulib_verttext_destroy(mulib_verttext_list_t *list);
```

## Behavior and limitations

Rectangle-based prototype data model; does not yet implement full advanced layout policy.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-verttext.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-verttext.c -lm -o test
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
| Header | [`include/mulib-verttext.h`](../include/mulib-verttext.h) |
| Implementation switch | `MULIB_IMPL_VERTTEXT` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-verttext.c`](../examples/mulib-verttext.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-verttext.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
