# objfileleader

Historical filename for an ELF PT_LOAD copy/zero-fill helper, **not execution**.
`load` accepts EXEC/DYN views, a destination virtual base and caller memory size.
It prevalidates every load segment's bounds and rejects overlapping memory
segments before copying. File-backed bytes are copied, BSS zeroed, gaps untouched.
Source bytes and destination must not overlap. Returns 0 success, ELF error otherwise.

No memory mapping, permissions, interpreter, dynamic linking, relocation or
entry-point execution. Caller must retain validated immutable ELF input.
Switches: `MULIB_IMPL_OBJFILELEADER`, `MULIB_IMPL_ELF`, `MULIB_IMPL_ENDIAN`.
Example: `examples/modules/objfileleader.c`.
