# Module metadata and the PPX boundary

`meta::encode_source_module_metadata` accepts a **parsed** `Unit` and returns a
`SourceModuleMetadata` packet. Its independent version is `1`. It records an
optional module header, structured module and program import requests with
source-spelled targets, selectors and aliases, and explicit declaration visibility. `SourceShared`
stores the positive, one-based source elevation; omitted `shared` and explicit
`shared elevate(1)` have the same representation. `Loc` values are informational.

`Unit 1` retains its existing declaration list and is still the ordinary PPX
protocol. Neither `--emit-ast` nor ordinary processors receive the separate
metadata packet. The host restores matching declaration metadata from its
original source unit, not from output locations. A synthesized declaration does
not inherit visibility. The loader guards graphs requiring module-scoped name
resolution: ordinary Unit-v1 processors cannot run on these graphs because
their returned names cannot be safely re-resolved against the original
module scopes.

An ordinary extension schema can add namespaced syntax, but cannot extend
protected roots such as `Unit`, `Location`, source-module metadata, or native
extension/evidence categories. Host protocol nodes are also rejected when
embedded in a Unit-v1 `Syntax` field. Native declaration expansion still uses
the existing `ModuleSyntax` and `ModuleEvidenceExpansion` packets; the host
checks returned source locations against the owner's carrier tokens and
endpoints before admitting generated definitions.
