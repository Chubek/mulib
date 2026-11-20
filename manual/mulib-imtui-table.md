# imtui-table — Virtualized table

Tables extend the list state and visible-row provider with columns and
header labels. Data stays in the caller's model. Render requests are bounded
by the visible table; a provider need not keep a full table of display cells.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-imtui-table.h`](../include/mulib-imtui-table.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_TABLE` |
| Repository-local includes | [mulib-imtui-list](mulib-imtui-list.md) |
| Complete example | [`examples/imtui/composites.c`](../examples/imtui/composites.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/imtui/composites.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
