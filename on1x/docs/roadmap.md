# On1x roadmap

Updated **2026-11-26** against the [qualification base and active changes](release-qualification.md).
The [capability matrix](capability-matrix.md) distinguishes an implemented
path from a dated, configuration-specific test result. The [verification
index](../VERIFICATION.md) links to earlier recorded work; it is not a
sign-off for the current tree.

## What already exists

- **Compiler and tooling.** The C++20 compiler supports a generated Aurocks
  scanner and an independent fallback scanner, bounded checking, explicit
  PPX, and AOT staging. There are working formatter, project, locked Git,
  diagnostic and editor tools, and a limited stdio LSP. Native PPX bootstrap
  is not compiler self-hosting. See [language](language.md),
  [formatter](formatter.md), [diagnostics](diagnostics.md) and [project
  policy](../stdlib/on1xfile/README.md).
- **Several artifact routes, not interchangeable.** Direct C11 is the
  checked-HIR reference path. MIR text/CFG are inspection products; the
  evaluator and MIR-C11 execute or emit a narrower scalar/trivial-product
  subset. VMWeave produces O1VM for its runner; checked-source VM lowering
  projects some copyable aggregates and closures, not nominal ownership.
  Portable MIR v4 contains directives; executable consumers must specialize
  them, and O1VM admits the executable v1–v3 subset. Generated Archon emits
  assembly for its configured native target; the dated execution evidence is
  Linux AMD64 SysV. Cephyr's optional hosted C11 object route and ArchVirt's
  KVM/DomIOS guest route are distinct. See [phase routes](backend-phase-audit.md),
  [MIR](../mir/README.md), [Cephyr](cephyr-ffi-module-verification.md) and
  [ArchVirt](archvirt.md).
- **Bounded ownership and concurrency.** Direct C11 supports lexical/shared
  or exclusive named-place loans, finite recursive unique owners, function
  defer, and consuming owner-bearing nominal `Result?` in their documented
  scopes. Eligible scalar and copy-tuple lexical loans lower to typed MIR
  slots, not a loan opcode. Cooperative CSP has typed pointer-free channels,
  cancellation, deadlines and restricted timer-only stackless frames; general
  async, ownership across resumption and native workers remain open. See
  [ownership routes](ownership-backends.md), [defer and errors](defer-errors.md),
  [CSP](concurrency.md) and [async frames](async-frames.md).
- **Distinct foreign-call paths.** Registration and target-specific CDecl
  inspection are not admission. Build-time portable `stdffi` recipes,
  explicit raw or exact review-gated KernelBind calls, and hosted load-time
  packages are distinct. Earlier Linux AMD64 load-time clamp qualification
  does not establish arbitrary kernels, targets, or production approval.
  See [migration and boundaries](ffi-migration.md), [KernelBind review](kernel-review.md)
  and [load-time evidence](kernel-load-qualification.md).
- **SDK and embedding.** Compiler SDK 2 and the distinct On1xRT ABI 1 are
  present. Linked backend discovery, request admission, toolchain readiness,
  and executed results must stay separate; no SDK view grants authority to
  publish checked IR. The runtime host's authorization does not provide a
  sandbox for native callbacks. See [SDK capability data](../sdk/CAPABILITY_MATRIX.json)
  and [On1xRT](../runtime-sdk/README.md).

## Next implementation and qualification gates

1. **Keep build and installation reproducible.** Record independent
   generated and forced-fallback profiles, host generator/Cephyr provenance,
   snapshot parity and restricted package inputs. Exercise the installed and
   moved-prefix consumers from outside the source tree. Record unavailable
   dependencies or unregistered tests rather than treating them as passes.
   The current [release checklist](release-qualification.md) tracks these gates.
2. **Qualify backend reporting at its real boundary.** Report the exact
   linked provider and request envelope (phase, representation/schema,
   artifact and target). Do not use the linked inventory to imply direct CLI
   route coverage, optional tools or successful execution. Expand product,
   target and construct coverage only after positive and fail-closed tests
   exercise those named paths.
3. **Keep explicit-owner MIR bounded.** The qualification's first candidate
   slice is separate from existing source-to-C11 ownership and copy-only
   VM projection. Admit only the explicitly qualified in-memory profile
   if its verifier and consumers pass. Source lowering, saved/wire
   formats, SDK/VM/Archon execution, aggregate memory ABI, general loan
   state, control-flow lifetimes, defer and owner-bearing `Result?` each
   need separate design and evidence. See the
   [release checklist](release-qualification.md) for the exact profile.
4. **Qualify foreign steps independently.** Compare a real admitted binding
   with an independent oracle and test rejection before a call or artifact
   publication. Keep contract review and linkage-specific evidence, separate
   from discovery or test keys. General pointer/resource ABI, ownership and
   error lowering, retained/foreign-thread callbacks and target coverage
   remain open.
5. **Extend language and application profiles without broadening existing
   claims.** General iterators, mutable or borrowed arrays, richer data
   derives, closure/aggregate MIR, persisted interfaces and functors,
   independent effect/ownership checking, general async, compiler bootstrap,
   and substantial managed/concurrent applications each need an explicit
   profile and executable tests. Existing workloads are not generalized
   benchmarks or a performance ranking.

## Boundaries that do not change

- ExoML owns canonical C ABI discovery and contract data; MuLIB owns C
  implementation and its resource semantics.
- On1x owns source and IR admission, type/effect checking, and verified
  artifact production. Authorization and successful compilation are not
  interchangeable.
- Neither discovery nor emitted metadata grants a safe foreign call.
- A build-host library is not an implicit runtime dependency of ordinary
  AOT output. An explicit native package, runtime SDK or VM route has its
  own linkage and execution boundary.

For historical context, the previous roadmap contains the
[zero-cost expression, native ABI and benchmark narratives](https://github.com/Chubek/mulib/blob/26c4fc0845be2ffd120ce0d68b4b7c5ee7c95be9/on1x/docs/roadmap.md#L13-L59),
the [earlier feature inventory](https://github.com/Chubek/mulib/blob/26c4fc0845be2ffd120ce0d68b4b7c5ee7c95be9/on1x/docs/roadmap.md#L181-L244),
and the [P0–P5 and backend milestone numbering](https://github.com/Chubek/mulib/blob/26c4fc0845be2ffd120ce0d68b4b7c5ee7c95be9/on1x/docs/roadmap.md#L246-L319).
The [archived case index](capability-matrix.md#historical-cases-and-measured-profiles)
links directly to selected earlier profiles rather than carrying their
configuration-specific claims forward.
