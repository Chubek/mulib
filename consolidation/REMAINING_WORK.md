# Remaining acceptance gates

The following architecture gates cannot be inferred from inventory or unit-test
success. Keep them open until the corresponding executable tests exist:

- Independently approved aggregate/resource KernelBind imports with checked
  construction, borrowing, invalidation, and destruction on failure. Test
  signatures and ephemeral signing keys are not production review.
- A generalized On1x managed data and owner model covering collections,
  resource-bearing elements, loans across supported calls, and deterministic
  cleanup. Preserve the current compiler's rejection of unsupported flows.
- MIR parity for managed values, resource effects, foreign calls and suspension.
  Direct HIR→C11 remains the reference; structural verification alone does not
  establish semantic equivalence.
- Structured ABI-2 facets, PPX evolution and fail-closed worker verification
  beyond the explicitly tested profiles. Trusted native loading is not worker
  isolation.
- Generated On1xRT module export, typed managed/owned message transfer, native
  concurrency and async cleanup need separate executable evidence.
- Application-sized On1x workflows require tested public resource paths across
  the relevant kernel families. A C-only pipeline, a discovery-only
  declaration, or a CPU media fallback does not establish those milestones.
