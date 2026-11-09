# mulib-chart

[Manuals](README.md) · [Library index](index.md)

Flexible SVG charts driven by **Mune**, with the original
[lcharts library](../library/lcharts/README.md) automatically installed. Builtins
and novel user charts share the same registry and SVG constructors. No third-party
chart library, browser, Lua runtime or external renderer is needed.

## Integration and ownership

Define CHART, MUNE, JSON, XML and their transitive implementation switches once.
The [example](../examples/modules/chart.c) includes the complete explicit
[notation recipe](../examples/notation-common.h). Mune's own recipe is in
[its manual](mulib-mune.md). On Linux link math, threads and the dynamic loader.

`create` copies Mune runtime limits and creates an independent state, loading
lcharts from embedded source. It returns NULL with an owned diagnostic on failure.
`destroy(NULL)` is harmless. The Lua state accessor is borrowed and must not be
destroyed independently or retained beyond its chart context. Same-context public
operations cannot be reentered; callers synchronize shared state. Native host
bindings are explicit, not automatically loaded.

`render(context,kind,data,options,error)` borrows JSON for the call and returns
an owned SVG element or NULL. Destroy it with `mulib_xml_destroy`. Results are
independent of the context/JSON. NULL options means an empty options table; data
must be non-NULL. JSON arrays/objects become ordinary Lua tables and JSON null is
the singleton `lcharts.null`. Node/depth limits are 100000/64; individual JSON
strings are <=1 MiB and Mune's heap budget bounds the complete bridge. No partial
SVG escapes. Diagnostics copy their text and remain valid after later executions.

## Builtins

All accept arrays of records, e.g. `[{"label":"A","value":30}, ...]`. Most also
accept plain numeric entries. Record color overrides the palette. Dimensions
default to 640x400, must be finite and between 100 and 16384. Common options are
width, height, title, colors, and background. Labels are literal XML-escaped text.
Builtin data/range coordinates have magnitude <10^100; native SVG numeric
attributes are bounded to 10^100. Point radius is positive and <=16384.

- **pie/donut**: nonnegative finite values and at least one positive value.
  Totals normalize before summation to avoid overflow. A full-circle single
  slice uses a circle, not a degenerate SVG arc. Donut hole is in (0,1), default .5.
- **bar**: positive/negative values, zero baseline and category labels.
- **line**: optional x, otherwise one-based positions; input order is preserved.
- **scatter**: x/y coordinates (y is required); optional radius.

Cartesian options ymin/ymax set an explicit increasing range; out-of-range values
fail rather than being silently clipped. Colors/stroke/radius are SVG-style
presentation inputs, not a typed CSS engine. Builtins have <=4096 points and
simple axes/labels/legends. Empty Cartesian data produces axes; empty pie fails.
There is no auto-pagination, label collision solver, statistics, date axis, 3-D
renderer or automatic aggregation. Large legends/long labels can exceed the canvas.

## Custom and novel charts

Pass <=1 MiB extension chunks to `extend`. Global `lcharts` is already installed;
`require('lcharts')` is optional. Use `register(name, function(data, options) ... end)`.
Registration replaces that context's existing kind, including a builtin. Lua
side effects persist after errors. Custom data can be arbitrary bounded JSON,
not just builtin point records.

Constructors svg, element, text, rect, circle, path and line create ordinary
`{tag,attrs,children}` tables; nodes support `node:add(child)`. Use
`lcharts.number` for locale-independent numbers embedded into path strings.
Custom plugins can implement arbitrary SVG compositions, including gradients,
groups, legends, coordinate transforms, polar charts or entirely new encodings.
There is no C-side chart-kind switch to extend.

Return one SVG-rooted tree. Native conversion bounds nodes, depth and 8 MiB of
names/attributes/text, rejects cycles/sparse arrays/invalid XML, and accepts the
documented SVG construction family (shapes, text/groups/defs, gradients, patterns,
masks, clipping, symbols/use and font elements). Event attributes and nonlocal
href references are rejected. Path/CSS semantics remain author-owned; this is not
a general browser security sanitizer or hardened execution sandbox.

Default states remove io/os/debug/package and dynamic/file loading, print/warn;
restricted require exposes only lcharts. Mune's separate heap/step/call/result
budgets apply. User bindings can add capabilities explicitly and must enforce
their own authorization. No file/network access or external command is invoked
by the chart library.

Evidence: [functional/novel chart tests](../tests/test_notation.c),
[allocation checks](../tests/test_notation_alloc.c),
[SVG interoperability](../tools/check_notation.py), and embedded-source checks
with `python3 tools/embed_notation.py --check`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-chart.h`](../include/mulib-chart.h) |
| Implementation switch | `MULIB_IMPL_CHART` |
| Repository-local includes | [`include/mulib-chart-data.inc`](../include/mulib-chart-data.inc), [mulib-json](mulib-json.md), [`include/mulib-mune-markup.inc`](../include/mulib-mune-markup.inc), [mulib-mune](mulib-mune.md), [mulib-xml](mulib-xml.md) |
| Complete example | [`examples/modules/chart.c`](../examples/modules/chart.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/chart.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
