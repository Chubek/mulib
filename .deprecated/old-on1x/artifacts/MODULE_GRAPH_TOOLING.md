# Module graph tooling v1

`on1xc --emit-import-graph FILE` and `--emit-module-graph FILE` default to
canonical JSON. Select `--graph-format sexpr` for interchange or `dot` for a
visualization. Pass normal loader options including `--project-module-index`,
`--no-prelude`, `--stdlib` and search paths. `--deps` and the project tool's
existing `--project-module-graph` v2 build checks remain separate contracts.

Strict resolution must succeed before output is published. `--graph-partial`
(JSON only) turns a located loader rejection into `on1x.graph-failure/v1`:
see `grammar/graph-failure-v1.schema.json`. A failed transaction contains no
admitted nodes, edges or snapshot. The diagnostic reports its identifier,
source basename (display only), and decimal zero-based UTF-8 byte offset.
Invalid options and internal failures are still errors. Partial SCCs and
precise explanations are not recovered from a failed transaction.

Graph publication refuses to replace the entry, membership index, explicit
interfaces, or any source/interface visited by inspection. The loader enforces
the destination exclusion even when a later resolver error would otherwise
produce `--graph-partial` output. Symlink and nonregular destinations reject;
staged files are removed on failure. Graph inspection also rejects independent
checker/backend requests rather than silently ignoring them.

`--emit-ppx-graph-v1` explicitly requests `(SourceModuleGraphMetadata 1
"import JSON" "module JSON")` from one successful snapshot. The encoded
S-expression is limited to 4 MiB. The packet is host-owned, may be inspected
without launching a transformer, and cannot be submitted as a Unit or added
as an extension field. This is an export protocol; negotiated module-aware
transformation/readmission is not implemented. Unit-v1 stays fail-closed.

Qualification commands (full driver requires the external compiler assets):

```
python3 on1x/addons/lsp/test_lsp.py
python3 on1x/sdk/tests/frontend/header_compile.py cc c++ on1x/sdk/include BUILD
python3 on1x/tests/test_graph_driver.py /path/to/on1xc
```

`tests/test_graph_packet.cpp` uses the actual wire parser and schema validator;
compile it with the configured `ppx_schema.hpp` and `src/{ppx_schema,sexpr,
numeric,type}.cpp`. The SDK module-logistics test exercises the opt-in C API.

### Project builds and graph v3

The project driver requests `--project-module-graph-v3` for staged member
and indexed target checks. This private absolute-path envelope includes the
existing physical v2 admission/cache inputs and the same transaction's logical
nodes in dependency-first order, fragment memberships and witnessed edges.
`grammar/project-module-graph-v3.schema.json` defines its shape. The driver
verifies exact staged membership, unique identities, path admission, referenced
endpoints, order, witnesses and consistency across per-entry inspections. Its
public report orders manifest display labels deterministically across those
inspections. It does not infer logical dependencies from physical pairs.
The original `--project-module-graph` continues emitting v2; a build requesting
v3 does not silently accept an older compiler response.

Integrated qualification at `662aa5ee` (W1 `9dab0cd6`, W2 `1928c04b`, W3
`e4437581`, W4 projection `4b00c2cd`) used the explicit external profile in
`MODULE_ALGEBRA.md`. Five CTests passed: `project_membership`, `graph_driver`,
`lsp`, `sdk` and `graph_packet`. The real driver fixture compared nested v2
facts with the standalone v2 command, preserved both fragment request witnesses
on one logical edge and consumed the v3 packet with the Python driver. SDK
refresh after removing an import changed the new graph and dependency count
without changing the old snapshot. `Modules.test_plan_projects_compiler_edges`
passed via the public project `plan` command using `--stdlib` pointed at the
trusted external root. `Modules.test_manifest_fragments_graph_and_atomic_publish`
reached native linking but failed because `tools/on1x-build.py` loads the
removed in-tree `stdlib/mulib/housekeeping.py`. The explicit source-library
option does not grant permission to import Python helpers from a project
stdlib. Full project executable publication remains unqualified on this lane.
