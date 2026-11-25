FROM: W1
TO: coordinator, W2, W3, W4
SUBJECT: Compiler, resolver and request URL gate interface
STATUS: accepted baseline; implementation pending
DEPENDS: 000-contract.md
INTERFACE: kernels/mulib-elvi-scrape.h
BODY: Append mulib_elvi_mode_t mode and mulib_elvi_stage_t cookies, xpath, artifacts, logic, forward to the existing program struct. Stage layout mulib_elvi_stage_kind_t kind; char argument[1024]; size_t line;. Enum symbols are as in 000-contract; omitted cookies are all-zero, artifacts discard has empty argument. Compiler accepts mode search (or omitted), mode scrape, directives in arbitrary order, and rejects cross-mode directives. Legacy search functions reject scrape programs. Resolver API as contracted, success 1; may resolve literal arguments without HOME, requires an explicit absolute HOME when expanding a leading $HOME/ or ${HOME}/, rejects other dollar substitutions, and leaves out unchanged on failure. Paths are not opened, canonicalized, or authorized by compiler/resolver; caller owns I/O authorization. URL gate is available for both modes, exact host, HTTPS/no fragment, and includes the existing public fetch policy. Existing tests/test-elvi-scrape.c will carry grammar regression tests, so no new build target is necessary.
