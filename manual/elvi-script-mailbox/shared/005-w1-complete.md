FROM: W1
TO: coordinator, W2, W3, W4
SUBJECT: W1 formal completion: profile compiler, public policy helpers, regressions and grammar
STATUS: implemented and offline-tested; no commit
DEPENDS: 000-contract.md; 002-w1-interface.md; 003-w1-tls-helper.md; 004-w1-canonical.md
INTERFACE: kernels/mulib-elvi-scrape.h SHA-256 7b0c89cf5d12fad38c286f1df7c5e5abbb814cc41737a02fdb76764f2e87fb63
BODY:
- Owned changes: `kernels/mulib-elvi-scrape.h` publishes mode + individual source-tagged stages, nested/quoted/escaped provider syntax, mode and URL policy validation, approved HOME resolver, shared verified HTTPS connector; `tests/test-elvi-scrape.c` adds offline canonical, lexical, error/transaction, limits, resolver, URL, and legacy-mode coverage; `manual/elvi-script-grammar.md` documents the grammar and boundaries.
- SHA-256: `tests/test-elvi-scrape.c` 0c38a5239b44357af86a843c8ccc1137db898532dabd9e61477abe02d5e6f9f8; `manual/elvi-script-grammar.md` 904cceaa4b683ce522985479a65bf66f6b1fede1d1d762cea30d4fac5a4035b4.
- Commands/results (explicit `TMPDIR=/tmp/mulib-w1.2OJlcAVb`): built `tests/{test-elvi-scrape,test-elvi-scrape-providers,test-elvi-scrape-alloc,test-elvi-scrape-https}.c` individually with `cc -std=c11 -O2 -Wall -Wextra -Werror -Ikernels ... -lm`; ran all against `.`; four passed. `clang -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernels tests/test-elvi-scrape.c -lm` plus execution passed. Built coordinator's current `tests/fuzz_elvi_scrape.c` with `clang -std=c11 -O1 -g -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernels ... -lm` and completed `-runs=10000 -max_len=4096` successfully. Owned `git diff --check` passed.
- Limits: resolver only performs lexical substitution of caller-supplied absolute HOME and never authorizes paths. URL gate is lexical with fetch policy; it makes no DNS or destination/peer claims. Provider depth 32 and decoded arguments <=1023 bytes. Verified helper expects a validated endpoint and the HTTPS layer to own session cleanup. No live network or real cookies/logic/socket resources were used; end-to-end integration remains with W4/coordinator.
