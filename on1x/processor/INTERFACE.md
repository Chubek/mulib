# Compiled processor interface (schema v1)

The engine and semantic validator include `<on1x/meta_schema.hpp>` and
`<on1x_meta_rules.hpp>` (generated in build/processor/generated).

Namespace `on1x::meta`:

* `Phase { Canonicalize, Desugar, Semantic }`.
* `NodeKind { Capture, Unary, Binary, Boolean, Integer, Unit, Select }`.
* `Node { NodeKind kind; const char* text; std::array<std::uint32_t,3> children;
  std::uint32_t arity; }`. Flat nodes; child indexes global in generated `nodes`.
  Capture text is binding name, operators/literals are text. `no_node=UINT32_MAX`.
* `Guard { Always, Boolean, Integer, Pure }`. Guard applies to all captures.
* `Rule { const char* name; Phase phase; std::uint32_t priority, match,
  replacement; Guard guard; const char* file; std::uint32_t line,column; }`.
* `Constraint { const char* name; const char* kind; const char* property;
  const char* value; const char* message; const char* file;
  std::uint32_t line,column; }`.
  Semantic expression-kind names match `Expr::Kind`; property vocabulary:
  `arity` (decimal exact children size), `min_arity`, `type` (bool/i64/unit),
  `resolved` (true), `callee` (present/absent), `forbidden` (true).
  Also `body` (present/absent), `operands` (boolean/same/numeric),
  `result` (operand0). Statement selectors use `StmtIf`, `StmtWhile`, etc.
  and allow `body`, `forbidden`, `value` (present/absent/boolean).
  Constraints only reject, never grant type/effect/ownership evidence.
* `Budgets { std::uint32_t steps,nodes,depth; }`.

Generated namespace `on1x::meta::generated` contains inline constexpr
`std::array<Node,N> nodes`, `std::array<Rule,N> rules`,
`std::array<Constraint,N> constraints`, `Budgets budgets`, `schema_version=1`.
`program_sha256` fingerprints the table payload, input hashes, schema header,
validator/emitter and generated parser; `parser_sha256` identifies the actual
LLSynth-generated parser bytes. Both are constexpr character arrays.
Rules sorted by phase then descending priority then stable name; names unique,
equal-priority overlapping patterns rejected. Capture relocation preserves AST
identity and provenance; newly constructed nodes inherit matched root location.
Evaluation order and capture multiplicity are validated by the generator.
Boolean identities/lazy Select lowering and checked-i64 neutral identities/
comparison duals are admitted; no arbitrary C++ actions.

Example `.o1m`:
```
meta 1;
budget steps 100000 nodes 1000000 depth 256;
rule bool_and_true phase canonicalize priority 100
match Binary("&&", $x, Boolean("true"))
replace $x
guard boolean;
constraint binary_arity phase semantic
match Binary require arity "2" message "binary expression needs two operands";
```

Build integration: `add_subdirectory(processor)` after frontend declaration.
The subdirectory adds frontend generation dependencies and private includes.
The `on1x_meta_generated` interface target is also available for internal
build-tree consumers; it is not an installed SDK dependency.
