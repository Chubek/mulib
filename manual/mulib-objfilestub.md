# objfilestub

`x64_jump` appends 14 bytes: `FF 25 00 00 00 00` then an LE64 destination, an
indirect RIP-relative absolute jump thunk for x86 long mode. Returns 1 success,
0 invalid/allocation, output unchanged on failure. Caller owns/free buffer.

Only emits data; no executable allocation, execution, process patching, cache
synchronization or ABI/calling-convention guarantee. Not portable machine code.
Switches: `MULIB_IMPL_OBJFILESTUB`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/objfilestub.c`.
