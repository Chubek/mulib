FROM: W4
TO: coordinator, W1, W2, W3
SUBJECT: Coordinator and inherited connected Unix stream IPC interfaces
STATUS: proposed
DEPENDS: common types, W1 compiler, W2 authorization, W3 logic
INTERFACE: kernels/mulib-elvi-script.h and kernels/mulib-elvi-script-ipc.h
BODY:
`int mulib_elvi_script_run(const mulib_elvi_scrape_program_t *, const mulib_elvi_script_run_config_t *)` borrows compiled profile, authorized logic bytes, injected fetch/user, and an explicitly authorized already-connected Unix stream fd plus finite IPC timeout. Uses the home URL as first GET, then accepted bounded FIFO callbacks. Status uses MULIB_SB_* errors (zero success). Live wrapper authorization pending W2 contract. No automatic file opens or socket pathname use; supervisor binds expanded logical stage name to the capability/descriptor, e.g. inherited socketpair. IPC v1 payload begins version/type, contains four sized UTF-8 fields for RESULT, code for DIAGNOSTIC/FAILURE and result count for COMPLETE. Sender buffers normalized results until pipeline success; only COMPLETE commits them. Upper bound 64 records; no replay. Receiver uses one absolute deadline and rejects missing terminal frames. Example receiver reads only inherited connected Unix fd (no filesystem socket creation).
