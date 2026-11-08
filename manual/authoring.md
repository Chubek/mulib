# Writing a library manual

[Manuals](README.md) · [Contribution guide](../CONTRIBUTING.md)

Use the implementation and its regression cases to establish the contract.
Keep existing filenames, including historical spellings, so inbound links work.
Compatibility pages should identify the canonical API and explain any additional
wrapper symbols or implementation switches.

## Contract coverage

Each manual should let a reader answer these questions without guessing:

- **Purpose:** What operations are implemented? Which format/version/platform
  profile applies? Which similarly named capabilities are outside that profile?
- **Integration:** Which header, switches, dependent implementations, native
  libraries, optional backends, and configuration macros are required?
- **Lifecycle:** How is state initialized and released? Can it be reused? Does
  initialization require fresh storage or release an existing object?
- **Ownership:** Are inputs copied or borrowed? When does insertion transfer
  ownership? What invalidates returned pointers? Who releases callback data?
- **Results:** What does each return value mean? Are absence and failure
  distinguishable? Are outputs unchanged, partial, or unspecified on failure?
- **Bounds:** What units do lengths use? Is a terminator included? Are empty
  input, NULL, overlap, overflow, and resource exhaustion supported?
- **Behavior:** What ordering, complexity, numeric conventions, serialization
  byte order, concurrency rules, and callback/reentrancy restrictions apply?
- **Evidence:** Where is a complete example, and which tests exercise the
  documented profile and failure cases?

Small value helpers may need only a few paragraphs. Larger parsers and runtimes
need explicit syntax/profile and lifecycle sections. Do not fill missing details
with library-wide assertions such as “all integer returns are Boolean” or “all
failures preserve output.”

## Examples and generated reference sections

Prefer a small complete program with checked results and cleanup. Clearly label
fragments that require surrounding code. The examples in `examples/modules/`
are the runnable source of truth; link to them rather than copying large programs.

`tools/manuals.py` owns only sections between its HTML markers and the alphabetical
index. Behavioral prose remains handwritten. A `manual-interface` marker opts a
page into a verbatim public-header excerpt; it retains comments and preprocessor
conditions rather than guessing a C declaration grammar. Edit the header to
change an excerpt, then regenerate it. Forwarding headers link their archived
implementation where applicable.

```sh
python3 tools/manuals.py --write
python3 tools/manuals.py --check
python3 tools/manuals.py --compile-examples
```

Run the relevant runtime regressions when changing behavior or examples. Link and
excerpt checks establish consistency, not semantic correctness or conformance.
