# MuWeb incremental MuLIB additions

For the full repository, see the [complete module index](index.md),
[integration guide](integration.md), and [data/search modules](data-search.md).

The [compiler and VM guide](compiler-tools.md) covers expression frontends,
control-flow analyses, register allocation, bytecode, and their regression suites.

This archive contains 35 implemented standalone building blocks, tests and manuals. It **does not** claim to contain every browser subsystem from the architectural roadmap. Native ECMAScript, complete HTML5, TLS security, WebDriver integrations and CSS rendering remain outside this archive. No fake or empty implementations are provided. Several lexical extraction helpers intentionally accept a narrow input subset and are not safe substitutes for a standards-compliant parser. Integrate with MuLIB’s existing Prowse, HTTP, HTML, XML, DOM and CSS libraries.

Run `sh tests/run-all.sh` at the archive root.

The original repository could be inspected through the GitHub connector but could not be cloned into the build environment because DNS/network access is disabled. Therefore these tests run standalone without upstream-header cross-integration.
