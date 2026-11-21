-- Ordered registration references for the built-in adapter archives.
-- This uses Yellowstone's manifest API, not a native ELF layout script.
qld.backend("c11", "on1x_builtin_c11_register")
qld.backend("mir-c11", "on1x_builtin_mir_c11_register")
qld.backend("mir-eval", "on1x_builtin_mir_eval_register")
qld.backend("mir-text", "on1x_builtin_mir_text_register")
qld.backend("mir-cfg", "on1x_builtin_mir_cfg_register")
qld.backend("mir-archon", "on1x_builtin_mir_archon_register")
