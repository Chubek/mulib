# Compiler-linked SDK backend report

`on1xc --backend-capabilities` reports the built-in SDK adapters in this
compiler binary. Use it as a stand-alone query in an SDK-enabled build.
The `on1x.backend-capabilities/v1` document uses the
same linked registrar map and descriptors as invocation. It does not register
providers, invoke backend callbacks, use a plugin registry, run tools, or
consume a program.

- **Discovery:** `linked-builtin` identifies an actual registrar reference.
  This scope excludes direct CLI dispatch, the MIR VM, external providers,
  foreign interfaces, and standalone build tools. A fresh public SDK registry
  does not automatically acquire these references. Absence in this report is
  not proof of absence from other routes.
- **Admission:** `phase`, `representation`, `input_schema`, `artifact`, and
  `target` describe the SDK selection contract. The schema numbers are
  descriptor bounds, not a promise about wire formats or individual constructs.
  For example, the lower bound `0` is a trusted descriptor convention, not an
  additional MIR wire version. `registration_prerequisite` reports the
  registrar's local precheck (when present), not a successful registration.
  A concrete program still needs canonical and backend checks; its admission
  remains `not-checked`.
- **Build/link readiness:** `compiler-linked` describes the adapter, not the
  output or external toolchain. `target.kind=independent` describes the SDK
  artifact selector; target-specific MIR directives can still affect a
  particular invocation. Archon `assembly` is not an object or executable;
  its exact target plus the explicit private host route are required.
  Assembler, linker and runtime availability remain `not-probed`.
- **Execution evidence:** `not-collected` is neither a pass nor a failure.
  `declared_deterministic` comes from the provider descriptor; it is not
  measured reproducibility. Qualification belongs in a separate ledger with
  concrete inputs, commands, targets and observed results.
