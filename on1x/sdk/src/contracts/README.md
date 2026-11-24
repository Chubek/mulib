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

## Compiler-owned FFIHandler descriptors

`<On1xSDK-FFIDescriptor.h>` is the SDK 2 read-only view over the canonical
compiler registry in `On1x::SDK`'s compiler-support archive. Use a length-aware
target and exact module/member names. A caller may pin the expected descriptor
ID or look up by ID alone. The view reports exact native primitive identity,
On1x surface types, target ABI, placement, and the validated scalar guards. Its
text borrows immutable compiler storage; it neither loads code nor allocates a
new registry or handle.

The descriptor identity does **not** bind a native object or confer authority.
Lookup does not authorize a source import, construct checked HIR from external
data, choose a provider, verify native bytes, or call a function. Use the
canonical build and independent trust path to execute an import. `ffi 1`
seed-only metadata is not exposed through this API. Existing CDecl, CDD and
structured-claim consumers keep their separate compatibility behavior; they
cannot register, override, or authorize these descriptors.
