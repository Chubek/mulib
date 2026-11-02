# Mach-O builder

Historical filename/switch retained: `MULIB_IMPL_MACHOBUIDLER`; API is
`mulib_machobuilder_text`. Also define `BYCUTILS` and `BUFFER`. Atomically appends
a deterministic x86-64 MH_OBJECT with one __TEXT,__text section, not an executable.
No symbols/relocations/signing. [Example](../examples/modules/machobuidler.c),
[contracts](storage-server.md#idl-and-native-output).
