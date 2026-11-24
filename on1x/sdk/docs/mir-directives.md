# Portable MIR directives in SDK 2

Canonical MIR version 4 adds a portable directive list. Use
`on1x_sdk_mir_snapshot_format_version` to inspect the canonical version
independently of the SDK ABI and the optional `O1XMIR1` envelope version.
Versions 1–3 still decode and report zero directives. Unknown canonical
versions are rejected. The envelope version and SDK ABI remain unchanged.

`on1x_sdk_mir_snapshot_directive_count` and `on1x_sdk_mir_snapshot_directive`
enumerate directives in wire order on both candidate and host-verified
snapshots. Initialize an `On1xSDK_MIRDirectiveView` with
`abi_version = ON1X_SDK_SEMANTICS_IR_ABI_VERSION` and
`struct_size = sizeof(On1xSDK_MIRDirectiveView)` before each call. An invalid
index returns `ON1X_SDK_INVALID_ARGUMENT`; an incompatible version or short
view returns `ON1X_SDK_ABI_MISMATCH`. Returned strings are length-aware,
borrowed from the exact snapshot, and valid while that snapshot is retained.
Function ID `0` denotes module scope. The views do not allocate or mutate IR.

Serialization through either SDK MIR wire mode and through the `mir-text`
built-in preserves all clauses. Decoded snapshots remain candidates and cannot
execute through `on1x_sdk_mir_evaluate`. That API is a portable reference
evaluator even for host-verified snapshots; it does not select a backend.

## Product selection and trust

A nonempty backend or target selector matches the exact selected provider name
or target triple, respectively; an empty selector is a wildcard. Matching is
case-sensitive and does not normalize triples. With no target supplied, only
wildcard target clauses match. The native `mir-archon` SDK provider continues to
require explicit selection and the configured, ABI-checked exact triple.

Compiler-linked built-ins specialize a private copy for the selected product;
the original portable snapshot is unchanged. `mir-c11` receives the portable
module so its emitter can also consider emitter-specific hints such as
`private-code` for hidden visibility on supported C toolchains. The public
backend request has no speculation-enabling flag: a speculative clause is not
an opt-in. The compiler's private dispatcher may opt in, but matching patterns
still require their canonical proof and verifier checks.

A hint may guide an already-admitted built-in's policy. It cannot select,
register, or activate a provider, grant an extension access to private compiler
MIR, enable native artifacts, bypass semantic checks, or admit foreign code.
Public extension backends continue to receive the portable serialized input
under their existing admission rules. The retired FFI descriptor functions
remain separate ABI compatibility entry points and do not become supported
through MIR directives.

The VM's `O1VM` format is executable bytecode, not a portable MIR container.
Its encoder requires MIR already specialized for the VM; its decoder rejects
canonical MIR v4 rather than treating a missing directive table as empty.
