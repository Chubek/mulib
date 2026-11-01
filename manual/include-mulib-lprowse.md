# Embedded lprowse library

`include/include-mulib-lprowse.h` is internal, generated C string data used by
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
