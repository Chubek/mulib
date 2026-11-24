# Structured foreign admission

The public `On1xSDK-{FFI,CDecl,ForeignType,ContractCheck,ContractReview,
Exotract,KernelBind}.h` interfaces are a bounded C11 projection of the existing
ExoML CDecl/contract and Exotract evidence. They do not parse C, resolve symbols,
or replace those canonical formats.

Admission is deliberately narrow:

- exact scalar layouts with scalar ownership;
- the ordinary target C calling convention (unknown conventions reject);
- immutable non-null C strings borrowed for one call, or immutable static
  string results;
- opaque-handle constructors, call-borrows, and destructors with explicit
  unique/consumed ownership and allocation/free effects. Constructor resource
  identity must exactly match the opaque CDecl nominal type.

Every admitted candidate must match its CDecl source revision, target, pointer
width, calling convention, contract digest, one-TU Exotract profile, review
subject, and executable-boundary evidence. Both host callbacks must authorize
the exact candidate. Unknown layout, ownership, effects, nullability, lifetime,
or callback behavior rejects. Extraction, shape validation, discovery, hashes,
and metadata are never admission by themselves.

The ABI-v1 `On1xSDK_Contract` adapter always returns `UNSUPPORTED`: its opaque
signature and ownership strings remain inspectable metadata and are never
reinterpreted as structured ABI 2 evidence.

`CddAccessors.c` is the single implementation TU for the MuLIB CDD catalogue,
plan validator, and checked borrowed accessors. Registry mutation invalidates
all returned pointers and plan indices.

## Retired compiler descriptor lookup

`on1x_sdk_ffi_descriptor_find` remains exported for ABI compatibility. Valid
queries return `ON1X_SDK_UNSUPPORTED`; the compiler no longer provides the
v2 registry. Structured host admission above is a separate API.
