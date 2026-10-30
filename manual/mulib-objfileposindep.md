# objfileposindep

`rel32` computes signed 32-bit S+A-P using checked unsigned address arithmetic.
Symbol/place are uint64, addend int64. S+A must remain a uint64 address; final
displacement must fit [-2^31,2^31-1]. P is relocation **field address**, not the
end of an instruction. Returns 1 success, 0 invalid/overflow; output unchanged.

This arithmetic primitive does not rewrite instructions, construct GOT/PLT,
parse relocation tables or certify a whole object position-independent.
Switch: `MULIB_IMPL_OBJFILEPOSINDEP`. Example: `examples/modules/objfileposindep.c`.
