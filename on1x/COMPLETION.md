# On1x implementation and acceptance boundaries

On1x aims to provide statically compiled, safe-by-default language facilities
and reusable access to MuLIB kernels. This is an acceptance guide, not a claim
that every capability below is complete. Current executable subsets and exact
historical evidence are in [VERIFICATION.md](VERIFICATION.md) and the
[capability matrix](docs/capability-matrix.md).

## Preserve the working compiler

The direct C11 backend remains the reference for supported language semantics.
Do not make optional native code generation, speculative optimization, or
synthesis a prerequisite for verified foreign bindings. Preserve parser
fallbacks, diagnostics, ownership rejection, cleanup, and reproducible builds.
See [BACKEND.md](BACKEND.md) and [MOONSTONE.md](MOONSTONE.md).

## Language and numerical profiles

Require real representation and code generation for admitted scalars, records,
enums, slices, typed errors, and owners. Unsupported shapes must fail closed.
Checked numeric conversions must reject out-of-range and nonfinite inputs
before narrowing. Results must only be published after kernel success.

The [numerical KernelBind profile](docs/kernel-numerics.md) executes selected
canonical Histprice and PriceOpt operations. Its embedded fixture checks
selection/holdout, optimization, and direct-C equivalence. That bounded evidence
does not establish general I/O, managed foreign owners, all pricing functions,
or production admission. Currency, precision, and chronology remain caller
domain constraints, not properties inferred from `double` arrays.

## Foreign-interface acceptance

Each safe import needs exact compiler-derived declarations, target ABI layout,
source and dependency pins, ownership/capacity/nullability/error/effect
contracts, compiler enforcement, executable differential tests, and independent
authorized review. A generator or test authority cannot approve itself.

Discovery, explicit raw adapters, executable test-reviewed profiles, and
production-approved safe imports are separate capabilities. Retain explicit
trusted/unsafe labels when independent approval is unavailable. Archive
generation must not turn static-inline inventory into a fabricated external
ABI. See [KERNEL_DIRECT_USAGE.md](KERNEL_DIRECT_USAGE.md).

## Resources and cleanup

Require move-only owners and bounded borrows where appropriate, deterministic
destruction, allocation-failure handling, thread affinity, and complete
cancellation paths. Test double destruction, escaped borrows, use after move,
out-of-bounds spans, wrong-thread access, and callbacks after release.
Do not replace recoverable errors with abort-only facades.

MIR support must independently establish ownership/drop verification, cleanup
edges, foreign effects, and suspension semantics before admitting those flows.
Structural validation alone does not establish parity with direct C11.

## Build and integration evidence

Use actual multi-module On1x sources, canonical public headers, declared
dependencies, and reproducible compiler commands. Test generated artifacts
without hand edits and compare against independent C oracles. Record compiler,
target, dependency revisions, commands, failures, skips, and unsupported
profiles. No hidden dynamic loading may stand in for static binding.

Application-sized workflows require public resource paths across the relevant
families. C-only fixtures and declaration discovery do not prove this. Optional
SDK, runtime export, worker isolation, and native concurrency each require their
own verification; do not promote one based on another's tests.
