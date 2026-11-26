# Native Aurocks integration

`grammar/On1x.g` is the only `.g` grammar owned by On1x. Its build uses the
C++ `aurocks` executable from LibStone `cornerstone/Aurocks`, with runtime
fault support from `1ebcf9bd2722577d894ff53f1ac017dc0009cde0` or later compatible
checkouts. The pinned LibStone `cornerstone/ASDL` native `asdlc` builds the
scanner and PPX schemas; those schema steps do not use Perl. Vendored grammars
and their generators are outside this migration.
The explicit bootstrap scanner does not consume or generate any `.g` file.

```sh
cmake -S on1x -B build/on1x-native -DBUILD_TESTING=ON -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_USE_YELLOWSTONE_LINK=OFF
cmake --build build/on1x-native --target on1xc on1x_scanner_tests on1x_generated_tokens on1x_reference_tokens
ctest --test-dir build/on1x-native -R '^on1x\.(aurocks_faults|scanner(_parity)?)$' --output-on-failure
```

For a cross build, first build `aurocks` and `asdlc` for the build host from
the pinned `cornerstone/Aurocks` and `cornerstone/ASDL` directories. Set
`ON1X_AUROCKS_HOST_EXECUTABLE` and `ON1X_ASDL_HOST_EXECUTABLE` to their absolute
paths in the On1x configuration. Do not substitute another revision just
because it reports the same tool versions.

The optional Yellowstone link pipeline defaults off when the separate,
vendored build still requires the removed `aurocks.pl`. This does not affect
the On1x scanner or the C backend's host-link path.

## Scanner boundary

The native parser's public `parse_*` functions consume a complete
NUL-terminated input and enforce a 1 MiB limit. They do not provide a
bounded terminal match or the Perl generator's former `save`/`attempt` cursor.
On1x accepts up to 2 MiB, preserves the exact token bytes, and uses shared
bounded recognition for nested comments, character literals and interpolating
strings. The grammar epilogue therefore uses the pinned portable emitter's
`aurocks_rx_match` and generated `aurocks_rules`/`aurocks_rhs` tables to match
regular terminals directly, without constructing an Earley chart. This is a
private emitter dependency, not a public Aurocks API. A compatible public
anchored longest-prefix API should accept a bounded byte span, identify the
terminal independently of its production index, and distinguish no match from
budget exhaustion. Re-run scanner parity whenever the pin or the grammar changes.

## Runtime lexical faults

```sh
on1xc --syntax-faults input.on1x > proposed.on1x
```

The command invokes the 120 anchored byte-PEG rules in `On1x.g` only at
positions the canonical scanner rejects. It emits Aurocks' patch messages,
notes, warnings, and errors on stderr. Diagnostic-only rules fail without
printing a proposal. A silent BOM repair and loud, unambiguous fullwidth
delimiter repairs are checked by rescanning before output. If a later error
prevents success, no partial proposal is output. Diagnostics use zero-based
byte offsets in the current candidate. Input must be smaller than 1 MiB,
and at most eight edits are proposed. The command never modifies the input
file, runs PPX, or claims a proposal is syntactically or semantically valid;
review it and check it separately. Normal builds and the bootstrap scanner
do not apply repairs.

Aurocks caps each grammar and runtime table at 64 rules. CMake runs the native
generator for both banks of the one authored grammar, then embeds the second
Aurocks-emitted table in the scanner. The first matching declaration wins
across both banks. The adapter uses `Aurocks::FaultRuntime`; it reuses Aurocks'
PEG matcher and patch engine rather than implementing a second matcher.
The canonical C++ parser still handles token-level syntax. For example,
invalid `+=` (two individually valid tokens) and a missing closing brace are
outside this lexical catalog.
