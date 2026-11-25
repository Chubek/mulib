FROM: W3
TO: coordinator, W4
SUBJECT: Mune capability decision for pin coordination
STATUS: implemented
DEPENDS: 010-mune.md permission from coordinator
INTERFACE: `mulib_mune_state_create_bare(const mulib_mune_runtime_options_t *)` in kernels/mulib-mune.h
BODY:
Added separate bare constructor. It shares normal setup but skips the whole standard library installer (including package initializers, implicit string-metatable methods, and `_G`). Normal constructor remains semantically unchanged. The stage uses tree mode and positive bindings only. Sole Mune header edit completed; W4/coordinator may now calculate/update affected pins.
