# mulib-docir

**Area:** Document IR  
**Implementation:** `MULIB_IMPL_DOCIR` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned semantic document tree for sections, paragraphs, text and links.

## API

```c
enum { mulib_docir_ROOT=0,mulib_docir_SECTION=1,mulib_docir_PARAGRAPH=2,mulib_docir_TEXT=3,mulib_docir_LINK=4,mulib_docir_LIST=5,mulib_docir_ITEM=6 };
typedef struct mulib_docir_node { unsigned kind;char *text;struct mulib_docir_node **children;size_t count,capacity; } mulib_docir_node_t;
mulib_docir_node_t *mulib_docir_new(unsigned kind,const char *text);
int mulib_docir_append(mulib_docir_node_t *parent,mulib_docir_node_t *child);
int mulib_docir_html(const mulib_docir_node_t *node,mulib_typecore_buf_t *out);
void mulib_docir_destroy(mulib_docir_node_t *node);
```

## Behavior and limitations

Owned semantic trees now enforce single-parent ownership and reject cycles and
corrupt public edges. The existing public node layout and kind values 0..6 are
retained. Nodes/fields are read-only borrowed views outside the mutation APIs;
create nodes through `new`/clone/deserialization, and release them with `destroy`.
Destruction and snapshot traversal are iterative and allocation failure is atomic.

See [the complete DocIR pipeline contract](docir-publishing.md) for new semantic
math/reference/figure/note/bibliography kinds, source/style metadata, normalized
cloning, snapshots, budgets, thread/allocator hooks, and safe HTML/LaTeX export.
This is not a positioned layout IR, native typesetter, or full standards profile.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docir.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-docir.c -lm -o test
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
| Header | [`include/mulib-docir.h`](../include/mulib-docir.h) |
| Implementation switch | `MULIB_IMPL_DOCIR` |
| Repository-local includes | [`include/detail/mulib-docir-core.h`](../include/detail/mulib-docir-core.h), [`include/detail/mulib-docir-render.h`](../include/detail/mulib-docir-render.h), [`include/detail/mulib-docir-validate.h`](../include/detail/mulib-docir-validate.h), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-docir.c`](../examples/mulib-docir.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-docir.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
