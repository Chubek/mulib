# Portable recipe companion, version 1

`<kernel>.portable-recipe.json` adds executable guards and optional directive
requests to an existing annotated ExoML v2 contract. It is a separate On1x
format, not an ExoML schema change. ExoML remains the source for C symbol
contracts, parameter positions, return, errors, effects, and provenance;
Cephyr supplies the resolved C interface and body. Do not turn ExoML prose or
catalogue candidate directives into recipe instructions.

The UTF-8 JSON object has four required fields: `schema_version` (integer `1`),
`library` (ExoML library/kernel ID), `revision` (the exact `sha256:…` bundle
revision from ExoML), and `functions` (an object keyed by C symbol). Include
exactly the exports being admitted through the reviewed portable profile.
Names, arities and types are not redeclared in the companion. Unknown
fields and duplicate JSON keys are errors.

Each function object requires a `guard`. Use `"true"` for an unconditional
guard, or a typed expression such as `["le", "$1", "$2"]`. Parameter
references are zero-based; literals are signed i64 JSON integers. `lt`, `le`,
`gt`, `ge`, and `eq` compare two i64 operands; `and` and `or` take two
predicates; `add` and `sub` take and return i64 operands. A guard must produce
a predicate; the expression size and nesting are bounded. A missing guard
never defaults to `"true"`. `clamp` explicitly checks `low <= high`, whereas
its saturating addition and subtraction need no precondition.

A function can also list `directives`. Omission or an empty array requests
no directives. Each object has `kind` (`"hint"` or `"rewrite"`) and `action`
from the ffispec v1 compiler's supported actions. Optional `backend` and
`target` are exact selectors; omission or `"*"` means unrestricted. The
`cold-code` and `no-inline` hints require `"backend": "mir-c11"`. Optional
`speculative` is a JSON boolean, defaulting to `false`. Requests are scoped
to one function, with no inherited selectors, and duplicate effective
requests are errors. The checked-in clamp requests explicitly preserve the
existing recipe's directives. Directive requests do not establish validity
of a guard, type, ABI, effect, or implementation.

## Loader and admission

`portable_recipe_contract.load_portable_recipe_contract(path, *, kernel,
revision, arities)` accepts the already verified ExoML library and bundle
revision, and a mapping from admitted C symbols to their verified arities.
It returns a mapping from those C symbols to `ExportRecipe` values. `.guard`
is a typed tree with tuples in place of JSON arrays; `.directives` is an
ordered tuple whose validated requests each have a `.render()` method.

The companion describes what to emit, not what to trust. Compare its guard
structurally against the independent `../reviewed.json` guard (normalizing
both to the same representation), and use the companion guard only if
they match. Keep the reviewed header pin, the ExoML admission checks, the
comparison against the Cephyr-derived body, and the ordinary ffispec compiler
checks. Removing or changing companion fields cannot broaden admission.
