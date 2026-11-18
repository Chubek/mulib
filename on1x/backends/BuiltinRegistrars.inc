// Portable fallback when no host Yellowstone manifest tool is selected.
ON1X_BUILTIN_REGISTRAR("c11", on1x_builtin_c11_register)
ON1X_BUILTIN_REGISTRAR("mir-c11", on1x_builtin_mir_c11_register)
ON1X_BUILTIN_REGISTRAR("mir-eval", on1x_builtin_mir_eval_register)
ON1X_BUILTIN_REGISTRAR("mir-text", on1x_builtin_mir_text_register)
ON1X_BUILTIN_REGISTRAR("mir-cfg", on1x_builtin_mir_cfg_register)
