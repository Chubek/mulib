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
