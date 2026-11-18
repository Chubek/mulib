# KernelBind: discovery and scalar C11 profiles

On1x recognizes `import kernel mulib.histprice as History;` and
`unsafe import kernel mulib.json;` as native, distinct import requests. `as` is
optional and defaults to the last component. `kernel` and `unsafe` remain
ordinary identifiers outside that syntax; in particular `import kernel;` is
still an ordinary source import.

Ordinary builds still reject imports during loading with
`ON1X-KERNEL-NOT-ADMITTED` or `ON1X-KERNEL-UNSAFE-UNSUPPORTED`, including
imports in dependencies. Two opt-in callable profiles exist: the original
explicitly unsafe profile below and review-gated [scalar](kernel-review.md) and
[numerical](kernel-numerics.md) profiles.
Reviewed builds require an operator-selected independent Ed25519 review authority;
the repository ships no production approval.

```on1x
unsafe import kernel mulib.clamp as Clamp;

fn main() {
    let answer = unsafe {
        let base = 40;
        Clamp.add_i64(base, 2)
    };
    println(answer);
    unsafe {
        println(Clamp.sub_i64(answer, 2));
    }
}
```

On native Linux x86-64, build with the Lua-enabled Exotract SDK configured below:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/kernel/clamp.on1x \
  --kernel-bind --on1xc build/kernelbind-gcc/on1xc \
  --exotract build/kernelbind-gcc/libstone-moonstone/exotract/exotract \
  --cc gcc --resource-dir "$(clang -print-resource-dir)" \
  --mulib-plan build/kernelbind-gcc/clamp-link.json -o build/kernelbind-gcc/clamp
build/kernelbind-gcc/clamp
```

Use fresh output paths. The clamp v2 profile exposes all three C functions:
`add_i64` and `sub_i64` have type `(i64, i64) -> i64`; `i64(value, low, high)`
has type `(i64, i64, i64) -> i64` and clamps to the inclusive interval.
On1x evaluates its arguments once, in source order, then checks `low <= high`.
Reversed bounds trigger an On1x assertion failure (exit 70), before the C
function runs. This check also applies to unsafe imports.
Aliases are local to the importing source file. Calls are direct only
and require a lexical `unsafe { ... }` block. In expression position, an
tail expression without a semicolon supplies the block's value; a tail followed by
`;` is a statement, and a block without a tail evaluates to `void`. Local
bindings are visible to the tail but not outside the block. Statements and
the tail run in order. `return` still returns from the enclosing function;
loop exits still target their enclosing lexical loop. No owner, borrow, or
concurrency handle can be yielded across this boundary.

An unsafe block does not grant permission to its enclosing expression (for
example, an unsafe argument does not authorize the outer call). A lambda does
not inherit permission from where it was created; give its body a separate
unsafe block if needed. The loader and checked HIR enforce these boundaries;
unsafe markers are not PPX wire data. Deferred calls, raw externs, macros, PPX,
staging, task groups, MIR and independent checker modes remain outside this
profile. An unsupported ABI stays unsupported in both block forms.

A reviewed program may use both import spellings under distinct aliases.
The unsafe alias still needs its own unsafe scope, even if the same kernel
also has a reviewed alias. Both aliases use the same implementation TU; the
reviewed bundle lists unsafe exports separately.

The build helper re-runs pinned ExoLib/Exotract declaration extraction, checks
the CDecl source, linkage, calling convention, arity, and signed 64-bit scalar
types, then compiles a typed `unsafe_foreign` caller. Generated adapters
include the canonical header and use `_Generic` and `_Static_assert` against
the actual C compiler. The existing planner supplies one `MULIB_IMPL_CLAMP`
translation unit. The optional link report records the CDecl fingerprint,
symbol and type IDs, target, input hashes and `safe_export:false`. The report
is not an authorization or a reusable compiler input. Standalone
`on1xc --emit-c --kernel-c11` emits the profile's C caller for inspection;
it does not run the SDK or link an executable.

## Inspect requests and discover declarations

From the MuLIB root (use a fresh report path):

```sh
cmake -S on1x -B build/kernelbind-gcc -DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_EXOML=ON \
  -DON1X_WITH_EXOML_EXTRACT=ON -DON1X_WITH_EXOTRACT=ON \
  -DON1X_WITH_EXOTRACT_LUA=ON
cmake --build build/kernelbind-gcc --parallel 4

build/kernelbind-gcc/on1xc --kernel-imports \
  on1x/tests/fixtures/kernel_discovery.on1x

python3 on1x/tools/on1x-build.py on1x/tests/fixtures/kernel_discovery.on1x \
  --inspect-kernels --on1xc build/kernelbind-gcc/on1xc \
  --exotract build/kernelbind-gcc/libstone-moonstone/exotract/exotract \
  --cc gcc --resource-dir "$(clang -print-resource-dir)" \
  -o build/kernelbind-gcc/kernel-discovery.json

ctest --test-dir build/kernelbind-gcc \
  -R '(kernel_imports|scanner_parity)' --output-on-failure
```

`--kernel-imports` needs no native SDK. It parses the ordinary source graph,
reports requests dependency-first (maximum 1,024 requests), and checks alias
collisions within each source file (including its ordinary imports and
declarations). It does not type-check the rest of the input and never runs PPX
or bundled native processors. Its `on1x.kernel-requests/v1` JSON records logical
names, aliases, source locations and source hashes. `file_hex` encodes the
canonical path's exact bytes; offset is zero-based, line and column one-based.

The separate build-helper inspection path passes canonical resolved paths to
the bundled `contracts-exotract/kernel-discovery-v1.lua` recipe. That recipe uses
the pinned LibStone `exolib.new`, `Preprocess`, `Parse` and `Bind` APIs. The
profiles are `mulib.histprice`, `mulib.statsprice`, `mulib.priceopt`,
`mulib.money`, `mulib.json`, `mulib.prettysexpr`, `mulib.ini`,
`mulib.cryptohash`, `mulib.socket`, `mulib.channel` and `mulib.clamp`. JSON attaches the
existing unreviewed ExoML v2 sidecar; PrettySexpr attaches its separate
unreviewed ExoML v2 sidecar. A safe clamp request attaches the new unreviewed
clamp sidecar. Review candidate and reviewed builds also attach the respective
unreviewed Histprice or PriceOpt sidecars. The other profiles and unsafe
clamp requests do not attach a sidecar. PrettySexpr requires the canonical
`mulib_pretty_parse_expr` and `mulib_pretty_render_expr` declarations. The
legacy PrettyPrint companion sidecar does not describe these declarations
and is not used. Repeated logical requests with distinct aliases share the
same extraction.
See [staged stdlib migrations](../tests/fixtures/kernel_migrations/README.md)
for proposed call sites and remaining ABI and ownership requirements.

The eight [sandbox kernel profiles](kernel-sandbox.md) additionally provide
declaration and archive evidence, but return `not_representable` and expose no
safe or unsafe callable imports.
The eight CDD modules are also discoverable as `mulib.cdd_catalogue`,
`mulib.cdd_registry`, `mulib.cdd_schema`, `mulib.cdd_query`, `mulib.cdd_plan`,
`mulib.cdd_resolve`, `mulib.cdd_execute`, and `mulib.cdd_diff`. These source names
map underscores to hyphens for the matching discovery-only MANIFEST entries.
The report returns `not_representable`, with CDecl metadata but no semantic
sidecar. Checked accessors for fixed arrays, counts and borrowed lifetimes are
required before preparing semantic contracts. CDD is not an archive or callable
profile. Other logical names are rejected at their source location, even if
they appear in the broader [MuLIB inventory](../contracts-exotract/README.md).

`--mulib-root`, `--mulib-path`, `ON1X_MULIB_ROOT`, and `ON1X_MULIB_PATH` select
the canonical checkout using the existing planner. Additional selected-header
candidates and escaped/symlinked canonical headers are rejected; paths do not
inject new C include directories. Profiles pull headers and implementation
switches from the same MANIFEST closure used by normal archive builds.
Compilers and Exotract are explicit trusted tools, and the supplied MuLIB
checkout, local tool files, compiler resource headers and host environment are
trusted build inputs. There is no network fetching or source-selected Lua,
sidecar, or PPX execution. Lua and libclang are build-time dependencies only.

The Linux-only publication uses atomic no-replace rename to emit a single
`on1x.kernel-discovery/v1` JSON report with ExoLib metadata, original Exotract
On1x metadata and embedded CDecl v2, normalized semantic sidecar (if present),
per-symbol `missing_review` or `not_representable` results (all
`callable:false`), and hashed inputs.
It checks declaration-only macro state, target, header identity, required
provenance pins and source/tool drift within the inspection invocation.
This report is **not** an import interface, build lock, ABI probe, or
authorization; neither On1x nor its build helper accepts it as a compile input.
It does not compile an implementation or publish an archive or executable.
Target compatibility tested here is native Linux x86_64 only. The report alone
does not prove ownership, error initialization, effects, or foreign-call
representability.

## What would unlock safe and resource-bearing imports

The [reviewed scalar](kernel-review.md) and [numerical](kernel-numerics.md)
builds implement independent signed authorization tied to CDecl, headers,
contracts, tools and target. Production use still needs independently issued
review: shipped sidecars are annotated, not approved. A
sidecar `review_status` string or the legacy `admitted-extern` mechanism does not
authorize a safe KernelBind import. The numerical path handles bounded
immutable slices and specific record/enum/status shapes only. Money, owned
handles, callbacks, and the full Shylock workflow remain unadmitted.
