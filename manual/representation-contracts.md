# Representation and execution contracts

These rules describe boundaries between reusable MuLIB kernels, language
values, foreign bindings, and native runtimes. They are not evidence that every
conversion or safe import is implemented.

## Representations

- Sparse storage and borrowed sparse views have different ownership. Use
  checked adapters that preserve lengths, index bounds, ordering, duplicates,
  explicit zeros, and physical units; a layout cast is not a conversion.
  See the [CSR manual](mulib-num-csr.md).
- Geometry and hardware conversions must preserve coordinate units, orientation,
  object identity, signal width, port direction, and unsupported-value policy.
  Equal field sizes do not establish interchangeability.
- Bytes carry explicit lengths and may contain NUL. C-string projection needs
  validation or a copying adapter. Never publish a temporary native pointer as a
  language value.
- Media timebases use seconds per tick. Frame views use plane capacities and
  byte strides; float image views use float-element strides and linear-light
  straight alpha. Packed native-endian GLX words are not byte-ordered media
  samples. Keep encoded/linear, packed/planar, and straight/premultiplied
  distinctions explicit.
- C owners, C views, language places, and engine-local handles are distinct.
  Publish results only after successful validation.

## Ownership and execution

- A newly initialized owner is unique until a supported move or retain.
  Destroy it at most once through its matching allocator/provider. Failure
  creates no live owner unless the API explicitly documents otherwise.
- Borrows do not transfer ownership. Shared borrows exclude mutation;
  exclusive borrows exclude overlapping access. Destruction, relocation, and
  reallocating mutation invalidate corresponding views.
- For supported named exclusive places, On1x `ref_replace(place, next)` holds
  the loan while evaluating `next`, then returns the old value. A typed early
  return during evaluation leaves the original slot intact.
- Cleanup follows actual language return/scope/deferral semantics. Terminal
  traps do not imply unwinding. Native callbacks must not retain call-scoped
  views or propagate C++ exceptions through C.
- A foreign binding pins exact declarations, target representations, source,
  dependencies, lifecycle, and effects. Discovery, raw execution, and safe
  admission are separate states.
- On1xRT copies descriptor metadata; callback code and context remain
  provider-owned. Arguments borrow handles for a call; successful managed
  results own one handle. Never transfer private generated-program pointers or
  unpinned borrows across suspension or thread boundaries.

## Evidence and remaining gates

Independent resource/aggregate admission still requires checked construction,
borrowing, invalidation, and cleanup. Test authorities and ephemeral signing
keys are not production approvals. MIR structural verification is not semantic
parity for managed owners, foreign calls, or suspension. Trusted native loading
is not worker isolation. Generated runtime exports, owned message transfer,
native concurrency, and async cleanup require their own executable evidence.

A C-only pipeline, discovery-only declaration, or CPU fallback establishes only
its documented profile. Refer to [On1x verification](../on1x/VERIFICATION.md),
the [memory model](../on1x/docs/memory-model.md), and
[KernelBind](../on1x/docs/kernel-bind.md) for implementation-specific evidence.
