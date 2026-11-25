FROM: W3
TO: coordinator, W4
SUBJECT: Logic implementation and verification complete
STATUS: implemented; offline focused tests passed
DEPENDS: common detail types, mulib-html/XML/textstream, mulib-xpath, bare Mune, fetch policy
INTERFACE: `mulib_elvi_script_logic_{create,response,finish,destroy}` and `mulib_mune_state_create_bare`
BODY:
Changed: kernels/mulib-elvi-script-logic.h, kernels/mulib-mune.h (approved narrow constructor extension), tests/test-elvi-script-logic.c, manual/elvi-script-logic.md, library/elvis/logic/BookingDotCom.lua. No other tree files changed by W3.
SHA-256: `432ae8ddfd3778901f34b5fe6e418defa516704b15d997f4f562d4cba8922bee` logic header; `380c07a00dfb561d13a6290625d960fb88946484fa5108c429779528d9990a43` Mune header; `1440ff049eea1d133caa0909bedd197310f4ba19e7c52bb7bbbc6012da12be75` canonical Lua.
Final API is baseline plus explicit `int mulib_elvi_script_logic_finish(mulib_elvi_script_logic_t *)`; create runs source plus optional on_start, response runs required on_response(document,effective_url), finish runs optional on_finish (no capability calls), destroy performs cleanup only. Native API and selectors per 002-w3-interface.md. Logic validates URL/typed fields; host callbacks own exact-host/no replay/transactional publication.
Verified: `TMPDIR=/tmp cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I kernels tests/test-elvi-script-logic.c -lm -ldl -pthread -o /tmp/mulib-w3-logic` then `TMPDIR=/tmp /tmp/mulib-w3-logic`; same source with `-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer`, run with `ASAN_OPTIONS=detect_leaks=1`; regular `tests/test_mune_runtime.c` with strict warnings and `TMPDIR=/tmp` passed. No live provider contact.
Limits: in-process capability restriction, not OS sandbox/wall-clock guarantee; authorized trusted source only. Full explicit-tag UTF-8 HTML (script/style/foreign content rejected), body 2 MiB, 512 nodes, depth 32, 4 KiB/tag; XPath restricted to 256-byte/8-step queries, single initial descendant axis and no parent, 64 matches/query, cumulative weighted work/512 generation-checked handles. Lua tree execution uses 16 KiB source, 2 MiB runtime heap, 200k steps/invocation, 32 call depth, no stdlib or implicit string metatable/package paths. Absolute HTTPS links only. Stage callbacks are synchronous and their duration and side effects are a host responsibility.
