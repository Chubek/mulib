# Embedded lprowse library

[Manuals](README.md) · [Library index](index.md)

`include/mulib-lprowse.h` is internal, generated C string data used by
`MULIB_IMPL_PROWSE_CONTROL`. Its source of truth is
[`library/lprowse/lprowse.lua`](../library/lprowse/lprowse.lua). It exports no
public API and requires no implementation switch. The static string array has
translation-unit-local linkage; split lines avoid compiler string-literal limits.
The controller concatenates it, evaluates it in Mune, and roots the returned module
in globals and `package.loaded.lprowse`.

Regenerate with `python3 tools/embed_lprowse.py`; verify with `--check`. This is
a development operation only. Runtime loading does not depend on a filesystem
Lua path, Python, or an external Lua installation. The source Lua file can also
be used by a compatible Lua host supplying the `__mulib_lprowse` native callback:
`(operation,json_arguments) -> value` or `(nil,message,status)`.

See [Prowse control](mulib-prowse-control.md) for the DSL, lifecycle, limits and
examples. Generated code and Lua source are original MuLIB MIT-licensed code.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lprowse.h`](../include/mulib-lprowse.h) |
| Implementation switch | None (support header; see scope above) |
| Repository-local includes | None |
| Complete example | [`examples/modules/prowse-control.c`](../examples/modules/prowse-control.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/prowse-control.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
