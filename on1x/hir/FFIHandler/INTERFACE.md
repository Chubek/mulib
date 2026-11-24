# FFIHandler descriptor interface v2

The build runs genuine LibStone LLSynth to generate a Python parser from
`FFIHandler.llsynth`, then `tools/ffic.py` validates its parse trees and writes
immutable C++ tables. Neither tool runs when importing an interface or invoking
a foreign function. Descriptors describe representation, not admission. A native
call additionally needs canonical HIR, applicable ABI checks, a native plan,
and the independent unsafe or safe admission policy.

## Source format

`ffi 2;` documents identify one exact target and one canonical module:

```text
ffi 2;
target "x86_64-linux-gnu" LP64 "SysVAMD64" sysv;
foreign mulib.clamp {
    provider mulib "clamp";
    header "mulib-clamp.h";
    fn i64(value: i64, low: i64, high: i64) -> i64 = c::mulib_clamp_i64
        native external declaration (integer(long,4,64,signed,8,8),
            integer(long,4,64,signed,8,8), integer(long,4,64,signed,8,8))
        -> integer(long,4,64,signed,8,8) profile scalar requires 1 <= 2;
}
```

* The other recognized target tuples are `aarch64-linux-gnu` / `LP64` /
  `AAPCS64` / `aapcs64` and `riscv64-linux-gnu` / `LP64` / `LP64` / `lp64`.
  Recognition is not executable qualification. Native and surface type slots
  are separate. Native forms are `void`, `integer(identity,rank,bits,sign,size,align)`
  and `real(identity,format,bits,size,align)`: identity and format use Cephyr
  atoms, sign is `signed` or `unsigned`, and size and alignment are in bytes.
  In particular `long`, `long-long`, `char` and `bool` do not collapse into
  another primitive of the same width.
* A typed `fn` is fixed and prototyped. Native `external declaration` and
  `external defined` denote external placement; `internal defined` denotes
  header static-inline placement and is only used for projected static inline.
  `profile scalar` allows exactly `i64`, `u8`, `u16`, `u32`, `u64`, `bool`,
  `f32`, `f64`, and `void` (result only). `i64` can represent signed primitives
  or unsigned integer widths below 64, with checked input narrowing as needed.
  Unsigned surfaces require equal unsigned native width; bool and float
  surfaces require their exact corresponding primitives and formats.
* The bounded scalar plan is fixed: evaluate all operands left-to-right once;
  perform position-based `requires N <= N` checks in declaration order; perform
  checked conversions; invoke one exact native call; convert its result.
  Guards refer to in-bounds positions of the same integral surface type.
  `effects unknown` is optional and the only accepted effect. No purity,
  initialization, retention or ownership may be inferred.
* `profile discovery` retains a primitive signature but emits no executable
  descriptor and takes no guards. `unsupported MEMBER = c::SYMBOL reason CODE;`
  retains an explicit non-executable symbol outcome; richer exact types and
  source provenance go in the generated target catalogue's index. A module
  may contain `unavailable reason CODE;` only, or no entries. Codes are stable
  lowercase snake_case identifiers. Rich types, resource IDs, companion
  recipes and marshalling operations are not executable in this schema.
* Module filenames, provider keys and header basenames retain exact manifest
  keys. Aliases replace `-` with `_`, `+` with `_plus_`, and prefix digit-initial
  keys with `k_`. Symbols normally use `mulib_<alias>_`; the only non-prefix
  exception is the existing `mulib_ct_counter_add` static inline in
  `mulib.container_core`. Spelling checks are not declaration origin evidence.

### Overrides

Authored deltas use the same grammar but a different document body:

```text
ffi 2;
target "x86_64-linux-gnu" LP64 "SysVAMD64" sysv;
extend mulib.clamp {
    select c::mulib_clamp_i64;
    fn c::mulib_clamp_i64 fingerprint "…64 lowercase hex digits…" requires 1 <= 2;
    obligation unknown_pointer_retention;
}
```

`select` is an explicit symbol selection for the generator to validate against
the canonical recipe and projection; it is not admission. `fn … fingerprint`
adds one or more position-based checks and does not imply selection. The
generator rejects stale and out-of-bounds deltas before producing the generated
document. `obligation` records an unresolved audit condition for discovery.
The compiler rejects an unmerged `extend` document. There is no DSL syntax for
executable code, invoking another tool, or granting safety.

### Identities and registry

For both identities below, SHA-256 hashes the ASCII output of
`json.dumps(value, ensure_ascii=True, separators=(',', ':'))`, without a newline:

* Native signature identity hashes `["on1x.ffi-native/v2", target, symbol,
  linkage, definition, native_parameters, native_result]`. `target` lists the
  triple, data model, ABI, then calling convention. Native types use arrays
  beginning with `"void"`, `"integer"`, or `"real"`, followed by the exact
  fields in DSL order. A typed `fn` implies fixed prototype/nonvariadic.
  Parameter display names and written storage do not participate.
* A foreign ID hashes `["on1x.ffi-id/v2", version, module, member, provider,
  header, native_fingerprint, surface_parameters, surface_result, profile,
  effects, guards]`. Parameters are surface atoms in argument order; guards
  are `[lhs,rhs]` pairs in check order. IDs are persistent description identity,
  not authorization. They bind neither source closure nor native artifact.
  The driver must verify both as part of its admission subject.

`<on1x/ffi.hpp>` defines the typed API: `foreign_descriptors()` yields the
immutable representation-eligible entries, `find_descriptor(module, member)`
and `find_descriptor(ForeignId)` resolve exact identities, and
`find_descriptor_by_symbol(symbol)` resolves an exact native spelling. They
return views into static compiler-owned storage; lookup does not authorize a
call. Canonical ingress must revalidate the identity, target, signature, uses,
and admission independently. Applicable C11 emission must also revalidate.

The legacy `ffi 1` seed grammar remains readable for compatibility. Its
descriptors are marked `schema_version == 1` and must not be treated as
projected native signatures. `<on1x/ffi_scalar.hpp>` is a transitional view:
it handles the old exact AMD64 `int64_t` shape only, with at most one guard.
Only the three prior clamp bindings retain that view when reading v2; new
typed exports cannot implicitly enter the legacy path.

## Build and bounds

The catalogue defaults to `on1x/stdffi/mulib`, with an alternative complete
target-qualified catalogue selected via `ON1X_FFI_CATALOG`. Its `index.json`
must match input file identities, hashes and explicit `ON1X_FFI_TARGET` (which
must match the Linux LP64 build target). The private descriptor reports hash
parser, validator and input identities, and list discovery separately. These
reports and the catalogue index are not an external compiler input at runtime
or a trust grant.

Files are capped at 64 KiB; each module has at most 256 entries, 16 native and
16 surface parameters, and 16 guards per function. Build shards compile eight
files at a time; the index has explicit limits of 8192 modules and 32768
representation-eligible descriptors, and validates duplicates across shards.
Discovery-only shards are not included in executable C++ tables. Source lists
are dependencies so removing a file cannot leave it active in the registry.
Package installation includes the built catalogue and index. Build outputs
are replaced atomically per file after validation; output pairs are not a
filesystem transaction. The generator and trusted parser are build tools,
not a sandbox for untrusted Python.

## Native graph discovery v3

`ffi 3;` uses the same target and module/provider/header envelope as v2.
Its body is either `unavailable reason CODE;` (optionally `audit "text"`
before the semicolon, at most 512 printable characters), or graph blocks
followed by `discover` statements. This is discovery only and cannot emit
an executable descriptor:

```text
graph 0 {
    tag 7 struct "node" complete (16,8) {
        field "value" at 0 : integer(int,3,32,signed,4,4);
        field "next" at 8 : pointer(64,8,8,tag-ref(7));
    }
}
discover example = c::mulib_example_example native external declaration graph 0
    function-type(fixed,(pointer(64,8,8,qualified(const,tag-ref(7)))),void)
    reason rich_lowering_unavailable;
```

* `tag ID struct|union|enum "CName"|none` ends in `incomplete;` or in
  `complete (SIZE,ALIGN) { ... }`. An enum's separate layout is always
  `(unknown,unknown)`: `underlying NATIVE_INTEGER;` gives its representation,
  followed by ordered `case "ENUMERATOR" = SIGNED_DECIMAL;` items. Same-width
  but different integer identities remain different. Records use ordered
  `field "CName"|none at OFFSET : TYPE;`; optional `bit-field(WIDTH,SHIFT,CONTAINER_OFFSET)`
  or `flexible` appears before the semicolon. All offsets are bytes except
  the bit field's width and shift. Recursive pointers are legal; by-value
  cycles, invalid field/array extents and dangling tags are rejected.
* Native atoms are the v2 primitives, plus `pointer(BITS,SIZE,ALIGN,TYPE)`,
  `array(COUNT,SIZE,ALIGN,TYPE)`, `tag-ref(ID)`,
  `qualified(const+restrict+volatile,TYPE)` with a valid nonempty sorted
  subset, and `function-type(fixed|variadic|unprototyped,(ARGS),RESULT)`.
  `unknown` denotes absent array size/count where projected. V3 retains,
  but does not execute, function pointers, enum/aggregate values, variadics,
  unprototyped declarations, bit fields and flexible array members.
  `specifiers(inline noreturn)` is optional before `reason`; the markers
  describe projection facts and do not authorize call or optimization.
* An `ipc-framed` AMD64 document may use `companion framed_ipc_v1;`
  before a sequence of private graphs, or `graph ID companion framed_ipc_v1`
  before the corresponding brace to mix distinct public/private graphs in
  one module. Only the eight known `on1x_kernel_ipc_v1_*` bridge spellings
  are recognized for that origin. This does not prove ownership, projection
  provenance, runtime artifact, callback lifetime, or authorization.
* Native graph types use the W03 `NativeGraph` type and tag shapes. A
  native fingerprint hashes compact, recursively key-sorted ASCII JSON
  `["on1x.ffi-native-graph/v3",target,source,symbol,linkage,definition,
  sorted_specifiers,normalized_root,normalized_reachable_tags]`, no newline.
  `source` is `["public",manifest_key,header]` or
  `["companion",manifest_key,recipe_key]`. Starting at the result and then
  parameters, assign tag IDs on first use, then visit the tag definitions
  in the same order. Graph IDs, original tag IDs, parameter names and
  unreachable tags do not affect a signature's fingerprint.
* V3 uses the same file and export budgets as v2: at most 128 tags per
  graph, 128 items per tag (including enum underlying type), 16 parameters
  per function node, nesting depth 16, and 2048 native type nodes per
  reachable declaration. The generated shard report retains these graphs
  and fingerprints for discovery, not for runtime lookup.

### Authored plans: validated numerical subset, no runtime publication

`tools/ffi_plan.py` parses and checks a bounded v3 `extend` subset, using the
same genuine LLSynth parser. `obligation CODE audit "text";` records a
non-authorizing module audit; text may be omitted. A plan requires explicit
`select c::SYMBOL;` and an `adapt c::SYMBOL fingerprint "DIGEST" …` matching
the exact discovery entry, module, and target. For example (not a projected
signature or a production override):

```text
adapt c::SYMBOL fingerprint "64 lowercase hexadecimal digits"
    surface (snapshot samples:slice(f64,256), value horizon:i64)
    -> sum(mulib.example.Result,nominal(mulib.example.Value),nominal(mulib.example.Error))
    effects unknown
    plan {
        snapshot(arg(0),local(0),1,256);
        slice_domain(local(0),finite_nonnegative);
        check_i64(arg(1),1,1000000);
        slice_data(local(0),native(0));
        slice_length(local(0),native(1));
        scalar_arg(arg(1),native(2));
        out_slot(local(1),native(3));
        init_field(local(1),field(0),nan);
        call(local(2));
        status_eq(local(2),1);
        out_field(local(1),field(0),local(3));
        check_field(local(3),finite_nonnegative);
        ok_record(local(3));
        reject(Rejected);
    }
```

* `arg`, `local`, `native`, and `field` are independent zero-based sorts.
  There are at most 16 surface and 16 native arguments, 32 single-assignment
  locals, 64 operations, and 256 elements per binary64 snapshot. No arbitrary
  expressions, C names, unbounded allocation, loops, or authority flags.
* Before the call, `check_i64(arg,lo,hi)` specifies an inclusive signed
  range. `check_f64(arg,zero_to_one|positive_to_one)` is ordered `[0,1]` or
  `(0,1]`. `snapshot(arg,local,min,capacity)` requires the backing, start
  and length checks before offset; `same_length(local,local)` and
  `slice_domain(local,finite_nonnegative)` address the checked windows.
  `slice_data` requires a pointer to const binary64 and `slice_length`
  exact C `unsigned long`. `scalar_arg` uses exact checked scalar mapping;
  `i64` to unsigned long additionally requires a preceding nonnegative
  check. Language-level argument evaluation remains once left-to-right;
  adapter rejection is not a language slice trap.
* `out_slot(local,native)` requires a mutable pointer to a complete named
  native struct with primitive fields. Every field gets explicit
  `init_field(local,field,zero|nan)` before `call(status_local)`; only a
  binary64 field can use `nan`. The one call requires all native slots
  assigned and surface slots used. Its status is exact C `int` and must be
  checked immediately by `status_eq(status_local,expected)`. Only its
  success continuation permits `out_field` and `check_field`
  (`finite`, `finite_nonnegative`, `is_nan` for binary64; `positive` for an
  integer). `ok_record` values follow the exact canonical record field
  order. Final `reject(case)` applies to every adapter rejection or native
  status mismatch; no such path reads or publishes output. Poison is not
  evidence that the native function wrote a field.
* `parse_rich_overrides` reads only authored data. `verify_plan` requires
  exact module-qualified nominal definitions resolved by its compiler
  consumer; the override cannot define or alias them. It returns a candidate
  digest over the target, module, symbol, native fingerprint, conservative
  effect, ordered surface modes and shapes, result identity, resolved
  nominal shapes, and typed operations, in compact ASCII JSON prefixed by
  `"on1x.ffi-marshalling-plan/v3"` as the first array element. It is **not**
  a foreign ID, artifact identity, or signed admission. Display names and
  audit prose do not affect it.

V2 remains the only projected executable *representation* supported by
`<on1x/ffi.hpp>`. V3 validation currently has no runtime publication;
`ffic.py` rejects unmerged `extend`. W06 canonical nominal/HIR resolution,
W07 lowering, and W10 native ABI, artifact and review support are required
before a validated rich candidate may become executable. Record/enum
input, resource ownership/loan or callback effects, retained roots, and
companion execution are explicitly unsupported by this plan subset. Keep
existing numerical and framed IPC adapters and their review/admission
mechanisms.
