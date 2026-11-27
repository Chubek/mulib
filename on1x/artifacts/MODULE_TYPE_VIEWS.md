# Executable type views: qualification slice

The direct-C11 loader permit accepts a transparent ascription of a monomorphic
record whose fields are numeric, Boolean or character values. The type remains
the original provider's nominal type; this is neither structural subtyping nor
an opaque handle. Only names present in the signature are projected, under
the original declaration visibility. Values use the existing direct-C11
record calling convention. These layouts have no destructor, failure cleanup
or move-only transport. Strings, generic records, enums, nested aggregates,
recursive and managed fields are outside this transparent profile.

The loader must export the verified transparent names separately from opaque
names and retain `Unit::direct_c11_type_views` for the entire combined unit.
MIR lowering and the Archon async projection reject that marker. The old
`require_executable_module_signature` API remains fail-closed for callers
that cannot install these gates.

Run the standalone admission test from the repository root:

```sh
c++ -std=c++20 -Ion1x/include on1x/src/{type,module_signature}.cpp \
  on1x/tests/test_transparent_permit.cpp -o "$BUILD/permit"
"$BUILD/permit"
```

On base `9180386bbfce6d141bf2707aabcecf0ed856af12` this passed plain and with
`-fsanitize=address,undefined -fno-omit-frame-pointer`. The existing
`test_module_signature.cpp` also passed when linked with the actual parser,
fallback scanner, signature resolver, extensions registry and type sources.
`test_module_signature_cli.py` now asks the real driver to resolve two aliases,
project a record field, compile the emitted C under strict C11, and run it for
`42`. This end-to-end assertion was **not executed** in the W2 worktree: the
checkout lacks the required compiler diagnostic catalog and formatter sources.
MIR gate syntax checked; optional Archon depends on an installed target-specific
`Backend.h` and was not compiled.

Manifest equations remain declaration-only; this patch does not admit them
into executable HIR. No change to the existing opaque ABI. Static functors,
managed/generic opaque transport and separate compilation are not delivered.
