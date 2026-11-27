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
`test_module_signature_cli.py` passed using the real generated Aurocks/ASDL
compiler with W1 loader `9dab0cd6` and W3 external-assets profile `e4437581`.
The explicitly selected assets were the unmodified stdlib tree `bb7e7814`
from external checkout `cbb7d66f`; LibStone was `4607ce58`. See
`MODULE_ALGEBRA.md` for the CMake options (W2 disables the SDK).

```sh
ON1X_TEST_VIEW_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 -B on1x/tests/test_module_signature_cli.py "$BUILD/on1xc" "$BUILD/fixtures"
```

Two aliases of the same exported record crossed a real provider call and
projected a field. Strict C11 compiled and the executable printed `42`, both
plain and under ASan/UBSan. A different provider's same-layout record failed
nominal unification. Opaque projection, MIR and VM emission all rejected and
preserved the prior output. Archon was not built.

## Scalar manifest equations

W1's loader adapter (`05e8b3f5`) and the manifest permit jointly admit a
transparent signature whose type members **all** normalize to numeric,
Boolean or character scalars. Every implementation equation must be listed.
For example, `type Word=i64; type Count=Word;` grants equality with `i64`,
including through a selective import alias. It does not create two nominal
record identities. Only authenticated type lookup substitutes the canonical
scalar, after ordinary visibility checks. C11 uses the existing scalar
calling convention; there is no move/drop or cleanup obligation. Missing
or inconsistent equations, undisclosed aliases, arguments to a monomorphic
alias, managed/string types, tuple equations, mixed profiles and opaque
manifest ascription are rejected for execution.

The CLI test above compiles and runs a two-fragment provider with `Count`
and `Word` and a consumer using both qualified and renamed types. The
strict-C11 executable prints `42` under ASan/UBSan. The test also rejects
stale equations, unlisted aliases, opaque ascription, invalid arity and MIR/VM
emission; earlier good C survives each failure. No change to the existing
opaque ABI. Static functors, managed/generic opaque transport and separate
compilation are not delivered.
