# MuLIB manuals

Start with the [complete module index](index.md),
[integration guide](integration.md), and [data/search modules](data-search.md).

The [compiler and VM guide](compiler-tools.md) covers expression frontends,
control-flow analyses, register allocation, bytecode, and their regression suites.

The [web/pricing integration guide](web-primitives.md) covers 67 granular modules,
their ownership and callback rules, supported subsets and security boundaries.
These integrate with the existing HTTP, Prowse, HTML, XML, DOM and CSS libraries;
they are not a claim of full HTML5, CSS rendering or ECMAScript conformance.

The [bounded graphics overlay](graphics-overlay.md) documents the shared CPU
renderer, native and Lua GUI layers, explicit presentation lifetimes, and the
device-free `make check-graphics` validation gate. Optional backend helpers are
not complete desktop protocols or GPU drivers.

The [Linux notebook desktop overlay](notebook-desktop.md) adds bounded notebook,
scene, window policy, X11, Wayland and execution kernels. Its separate device-free
build gate is `make check-notebook-desktop`.

The [secret kernel guide](secret-kernels.md) covers the development vault,
credential primitives, and their security boundaries and verification targets.

Every implemented module has a manual and a runnable example. Use each manual's
source/build reference for implementation switches and dependencies.

See [integration](integration.md), [module groups](modules.md),
[hardening](hardening.md) and [manual authoring](authoring.md) for repository-wide
build, verification and contribution workflows.
