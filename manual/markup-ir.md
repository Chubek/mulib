# MuLIB markup intermediate-representation kernel

## Intent and module map

These are **five ISO C11 single-header modules** under `kernels/`, designed
as an additive counterpart to MuLIB's `mulib-docir.h`, `mulib-dom.h`,
`mulib-syntax-tree.h`, `mulib-sexpr.h`, `mulib-graph.h`, and `mulib-evtstream.h`.
The supplied `mulib_mkir_` API remains available; new entry points use
`mulib_cdd_`.

| IR | API or module | Structural guarantees |
|---|---|---|
| Raw lexical stream | `mulib_mkir_scan_next` | Lossless source offsets, no allocations per token |
| CST | `mulib_mkir_cst_parse` | Owns exact source and token table, plus an ordered tree |
| DOM-like tree / flat arena | `mulib_mkir_tree_*` | Stable indexed nodes; mutable parent/child order and owned attributes |
| AST | `mulib_mkir_tree_ast` | Deep copy and optional comment/trivia pruning |
| S-expression / SXML-like | `mulib_mkir_tree_sxml` | Escaped textual export, owned result |
| Push/pull events | `mulib_mkir_events_*` | Zero-copy pull traversal with enter/leave/text/comment events; scanner tokens support push drivers |
| Text+stand-off ranges | `mulib_mkir_spans_*` | Owned text and arbitrarily overlapping half-open annotations |
| Attributed runs | `mulib_mkir_spans_runs` | Partitions text at all span boundaries, lists active annotations |
| DAG and general property graph | `mulib_mkir_graph_*` | Sparse directed typed edges; optional cycle rejection, topological order |
| Hypergraph | `mulib_mkir_graph_hyperedge` | Typed edge with any positive number of vertex members |
| Hybrid document | `mulib_mkir_hybrid_*` | Tree, stand-off spans, cross-references and overlap graph |

Modules and implementation switches:

```
kernels/mulib-markup-tree.h      MULIB_IMPL_CDD_MARKUP_TREE
kernels/mulib-markup-stream.h    MULIB_IMPL_CDD_MARKUP_STREAM
kernels/mulib-markup-spans.h     MULIB_IMPL_CDD_MARKUP_SPANS
kernels/mulib-markup-graph.h     MULIB_IMPL_CDD_MARKUP_GRAPH
kernels/mulib-markup-bridge.h    MULIB_IMPL_CDD_MARKUP_BRIDGE
```

The original `MULIB_IMPL_MARKUP_*` spellings remain compatibility aliases.
Define each used switch in **one C translation unit only**. The implementation
units may be combined or compiled separately. Other translation units include
declarations normally, including C++17 consumers. The bridge includes the
other four headers but does not automatically compile their implementations:
explicitly define all five switches when using the full kernel. All five depend only on libc;
no third-party libraries or linkage with existing MuLIB modules are required.

```c
#define MULIB_IMPL_CDD_MARKUP_TREE
#define MULIB_IMPL_CDD_MARKUP_STREAM
#define MULIB_IMPL_CDD_MARKUP_SPANS
#define MULIB_IMPL_CDD_MARKUP_GRAPH
#define MULIB_IMPL_CDD_MARKUP_BRIDGE
#include "mulib-markup-bridge.h"
```

## Memory and error contracts

- Initialize owner structs with `{0}`. Builders return `1` for success or `0`
  for error. Constructors and conversions publish the new object only on
  success; an invalid nonempty destination is not cleared.
  `scan_next` returns 1/0/-1 for token/EOF/error; `events_next` likewise for
  event/EOF/invalidation. `graph_path` returns 1/0/-1.
- Free with the matching `*_free` function. String pointers in trees, spans,
  graphs and CSTs are owned; `tree_sxml` returns `malloc` memory to `free`.
  Iteration and `get_attr` expose borrowed pointers.
- Tree node IDs, annotation IDs and graph vertex IDs remain stable across
  additions. `tree_attach` and `mulib_cdd_mkir_tree_insert_before` change
  traversal order but not node IDs or their recorded source offsets. An
  insertion may name an existing sibling as its position; a rejected move or
  a no-op move does not invalidate an iterator.
- `tree` and `spans` are independent representations. A hybrid **owns** both.
  `hybrid_index` replaces its graph; manually added references and anchors must
  be re-established afterwards. Graph vertex IDs are invalidated by reindexing.
- `cst.source` is a lossless snapshot of the input. Subsequent tree edits do
  **not** rewrite raw source or rebase source offsets. Use
  `mulib_cdd_mkir_cst_token_bytes` to borrow the complete source slice of a
  token without a copy or requiring a NUL terminator after the slice.
- Tree and graph validation functions check public arrays/links. Do not
  modify internal struct members directly except `graph.acyclic` before adding
  edges. Call the APIs for mutations and free only intact owner structs.
- Empty annotation ranges are allowed for positions (zero-width bookmarks)
  but ignored by run partitioning; `spans_to_tree` rejects them.
  `mulib_cdd_mkir_spans_runs_limited` lets callers cap both the number of
  produced runs and the sum of their active annotation counts before
  committing the result. A zero limit is literal. Active IDs are in insertion
  order; uncovered text still belongs to a run.
- Offsets are **byte** offsets, not Unicode scalar, UTF-16 or grapheme offsets.
  Calling code is responsible for UTF-8 codepoint boundary validation.
- The generic forest permits children on any node kind; the parser itself
  produces a more constrained tree. Document, element, and trivia nodes
  produce paired enter/leave events. Other nodes produce one event before
  their children, if they have any. Iteration of a subtree never continues
  into that root's following sibling or enclosing parent.
  `mulib_cdd_mkir_events_walk_bounded` takes a maximum callback depth and
  event count (`SIZE_MAX` means unlimited). It returns `-2` before delivering
  an excess event; limits do not skip initial tree validation.

## Conversions and lossiness

`tree_to_spans` concatenates text, CDATA, and trivia values in depth-first
document order. Every element creates one range over its descendant text.
Ranges are recorded in opening order, with an outer element preceding an
inner element when they cover the same bytes. Attributes,
comments and source formatting are not preserved in spans. The resulting
ranges are naturally laminar (properly nested), but users can add new
crossing ranges later.

`spans_to_tree` reconstructs element/text nodes only when all spans have
positive width and are properly nested. Crossing annotations fail without
publishing a partial output. Equal ranges nest deterministically in annotation
insertion order. The original text is preserved byte-for-byte. Graph conversion
is the non-lossy choice for overlap topology: annotation vertices connect to
runs by `covers` edges, with `annotation-set` hyperedges for active memberships.
`mulib_cdd_mkir_spans_to_graph_limited` applies the same run and membership
budgets as `mulib_cdd_mkir_spans_runs_limited` before constructing the graph.

`tree_to_graph` makes a DAG of structural `child` links. A second parent may
be added as another edge. Set `acyclic=0` for arbitrary references, which can
form cycles. Reachability and topological ordering follow only directed edges,
not hyperedge membership. If multiple vertices are ready in a topological sort,
the smallest ID wins; a cycle leaves caller output untouched.
A hybrid copies the complete forest without renumbering nodes: the selected
root controls which subtree is flattened, not which nodes are copied. Its
graph includes all tree-node vertices, span vertices, run vertices,
membership edges and hyperedges. Additional reference and anchor
edges can be added using `hybrid_reference` and `hybrid_anchor`.
New nodes and spans must be indexed before they can be used in these edges;
a failed rebuild leaves the previous graph and index offsets intact.

## Parser scope and security boundaries

`scan_next`, `parse` and `cst_parse` use a **deliberately small XML-like
syntax**, not full XML, HTML or Markdown. Supported: simple ASCII element and
attribute names, single-/double-quoted attributes, closing and self-closing
tags, comments, processing instructions, basic `<!...>` declarations,
`<![CDATA[...]]>` sections and literal text. The parser rejects mismatched tags and duplicate attribute
names; element depth, including self-closing tags, is bounded by the caller
(default 256). Attribute contents and
text are stored **verbatim** (no XML entity decoding); comments, declarations,
and processing instructions are retained as raw text nodes. CDATA content is
literal, with no delimiter bytes in the node value; the CST keeps the full
delimited token. HTML implied end tags, XML namespaces/DTD expansion, XML character legality,
Unicode element names and error-recovery parsing are **not** implemented.
Do not substitute this parser for a standards-compliant or security-reviewed
XML/HTML processor. Use the IR containers with an external production parser
if standards conformance is needed.

For huge or untrusted documents, add application-level byte, node, attribute,
and annotation budgets. The run and graph limits cap produced memberships,
not the cost of inspecting the input spans. Construction checks overflow,
graph cycles are checked when enabled, and parsing uses no recursion. Directed
reachability builds temporary adjacency; repeated insertion with cycle checks
and dense overlapping ranges can still be expensive.

## Compile

From the MuLIB repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels \
  examples/markup-ir.c -o markup-ir-example
./markup-ir-example
make check-markup
```

The focused runner covers the C tests and a separate C++17 consumer using the
legacy switches in its C implementation unit. Configure CMake with
`-DMULIB_SANITIZERS=ON` for ASan/UBSan or with Clang and
`-DMULIB_BUILD_FUZZER=ON` to build `mulib_fuzz_markup_ir`. The fuzz target
checks exact CST token coverage, compares stand-off run membership against
a direct interval oracle, and checks graph reachability and topological order
against an independent adjacency-matrix model. Seed it with
`tests/fixtures/markup-ir/`. The C
integration suite checks all 100 pairs of half-open ranges on three bytes and
injects individual allocation failures through CST construction, hybrid
construction, and hybrid reindexing.
