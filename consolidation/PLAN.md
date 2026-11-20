# MuLIB / On1x consolidation plan

Starting revision: `d1bddd5f5e4a41f8cc6231564a40d9463825bfa2`; configured
LibStone: `3399047f6c6328b8f07788795c610699e66c060a`. Keep the configured
LibStone checkout read-only. Run the starting builds in the original checkout
with external build directories; workers implement in detached worktrees from
the same revision and use the original LibStone path explicitly.

## Ownership

| Worker | Exclusive implementation scope | First integration target |
|---|---|---|
| 1 | MuLIB numeric, Amb3r and media representation code in `include/`; associated manuals and kernel fixtures | Checked conversion across a real representation boundary and an independent numerical oracle |
| 2 | `MANIFEST.yaml`, root build/catalogue tooling, generated dependency and MuLL artifacts, inventory tests | Restore generated inventory and prove declaration discovery does not create a call path |
| 3 | KernelBind and contract tooling, contract inputs, SDK contract components, FFI-specific tests | Extend the existing extraction/review/native-wrapper path for one bounded signature |
| 4 | Canonical language/ownership code including HIR validation, `codegen.cpp`, and non-SDK stdlib; focused language tests | Close a managed data or resource-cleanup gap through actual HIR→C11 execution |
| 5 | `on1x/src/backend/`, canonical MIR headers and MIR-specific tests | Add or harden one executable MIR subset with direct-C11/evaluator/MIR-C11 comparison |
| 6 | SDK compiler, facets, plugin, frontend/PPX and SDK stdlib; no shared driver/build surfaces | Run an explicitly selected extension through the canonical compiler and verification boundary |
| 7 | Private runtime and public `runtime-sdk/`, focused runtime tests and an isolated emitter/tool source if needed | Authorized native embedding or typed lifecycle slice without conflating private/public ABI |
| 8 | `consolidation/workloads/` and local fixtures | Exercise public interfaces against independent deterministic expectations; report blockers without bypassing admission |

Principal owns shared On1x build and CLI entrypoints, the SDK umbrella and top-level
architecture files, shared verification, the consolidated capability matrix and
worktree integration. File requests across boundaries go to the principal (e.g.
root CMake hooks from worker 1, loader changes to support worker 3, runtime
hooks from worker 4, MIR facilities needed by worker 6). The principal wires
new tests into shared On1x suites. No worker overwrites other worktrees.

## Shared boundaries

- Use the representations in [REPRESENTATIONS.md](REPRESENTATIONS.md) as
  starting points. Prefer checked adapters for different index widths, units,
  color/alpha conventions or ownership. Do not substitute compatible layouts
  for semantic equivalence.
- [OWNERSHIP.md](OWNERSHIP.md) defines the intersection supported across
  compiler and native boundaries. New contract metadata is candidate evidence;
  public safe calls still require target ABI evidence and independent pinned
  approval. Test keys may exercise that gate, not confer production approval.
- HIR→C11 is the reference. Keep the existing MIR verifier, effect order and
  wire versions intact; serializers and SDK transformations produce candidates
  that must go through canonical verification. Expose unsupported operations
  explicitly rather than erasing effects or cleanup.
- Public On1xRT handles are engine/domain/thread checked and distinct from private
  generated-program values. Native loading stays trusted and opt-in. Cooperative
  scheduling is not native parallel execution; payloads stay in the existing
  pointer-free subset unless the language, runtime and verifier agree otherwise.
- The manifest owns discovery, strata represent dependencies, implementation
  switches emit in one TU, and static inline has no external symbol without a
  wrapper. Regenerate dependent snapshots after signature or dependency changes.
- Application tests use deterministic local fixtures, negative/failure cases
  and analytical or independent oracles. They report discovery-only and
  unavailable providers separately from executed functionality.

Baseline and worktree audits precede parallel changes. Integrate kernel
representation changes before final inventory regeneration, keep admission
independent of catalogue discovery, and run the application checks only against
interfaces actually available in the integrated tree. See
[VERIFICATION.md](VERIFICATION.md) for baseline versus integration evidence.
