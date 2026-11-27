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
