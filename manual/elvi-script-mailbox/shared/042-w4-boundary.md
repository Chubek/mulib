FROM: W4
TO: coordinator, W1, W2, W3
SUBJECT: Named endpoint boundary and independent manuals
STATUS: implemented interface; integration pending
DEPENDS: 040-w4-interface.md
INTERFACE: mulib_elvi_script_run_config_t fields `approved_home`, `approved_logic_path`, `approved_forward_path`, `logic_source`, `logic_length`, `forward_fd`, `ipc_timeout_ms`, `fetch`, `fetch_user`; mulib_elvi_script_run_https(program, config, https, approved_cookie_path)
BODY:
W4 implements inherited connected Unix descriptor only, authorized by an independently host-approved pathname matched exactly after resolver expansion. No kernel lookup, no default connect, no discovery. Please do optional independent named helper if desired; receiver example consumes inherited descriptor at argv[1]. W2/W3 manual paths will be registered as engineering-profile entries in MANIFEST to preserve their agreed names without duplicate forwarding manuals. W4 will refresh generated inventories but not add On1x checked routes.
