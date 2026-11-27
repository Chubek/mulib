# Standalone module algebra profile

From the repository root, with LibStone at `4607ce58ea70482a9c057c64557260329d36afde`:

```
cmake -S on1x -B build/core -DON1X_MODULE_CORE_ONLY=ON -DON1X_LIBSTONE_ROOT=/path/to/libstone
cmake --build build/core
ctest --test-dir build/core --output-on-failure
cmake --install build/core --prefix /path/to/stage
```

This profile needs C++20 and the pinned header, not the deleted stdlib, generator tools, or a user-supplied policy path. The full compiler profile checks its deleted catalog, formatter and kernel-generation inputs at configuration. The core profile does not build `on1xc`. Full-compiler tests are not included in core coverage.

`src/logistics/module-language.oml` combines access, re-export and view judgments in one pinned language specification. CMake verifies the header and rule digests, embeds the exact rule text, and installs the source for inspection. `oml::rule_digest()` identifies that text; installed consumers link the compiled static library without including LibDSL. Vendored user code and source overlays cannot select rules.

## Judgments

`include/on1x/module_algebra.hpp` is a trusted compiler-internal C++ API. Pass original authenticated owners; do not reconstruct these values from graph or PPX serialization. An environment maps `(namespace, exposed spelling)` to the original declaration key, owner, visibility, type and seal. Only the host can supply authenticated binding facts. This API is not a validator of arbitrary external binding claims.

* `access(rule,owner,requester,admitted)` returns a `ModuleAccess` decision. A bounded AttributeEngine dependency graph derives the access decision from seven independent facts. One snapshot per judgment; keys are fixed attribute names, not pointers; no cache survives a transaction. OML determines precedence and the complete access decision. The adapter computes identity, validity and prefix predicates without I/O.
* `reexport(original,forwarding)` returns whether both access decisions allow forwarding. It does not assemble a re-export or grant access to another requester.
* `merge(left,right)` accepts identical bindings at a colliding name, rejecting distinct declarations and contradictory provenance; `Environment{}` is empty. `project(env,names,hide)` retains listed names or their complement. `rename(env,mapping)` requires existing keys, preserved namespace, and injectivity across the whole output, including unchanged names. `select(exports,selectors,requester,admitted)` requires an exact, accessible, non-repeated selector; it never uses a replacement owner. These host operations return new maps or throw without publishing a partial map. The caller resolves operator selector keys and authenticates export membership.
* The view operations cap collections at 4096 entries. Each policy query expands only pinned syntax with Boolean inputs and bounded LibDSL work. Source text is never interpolated. Host operations currently report exceptions, not rich located OML diagnostics.

W3 qualification at MuLIB base `9180386bbfce6d141bf2707aabcecf0ed856af12`: standalone CTest passed with GCC 16.2.1 (2,880 access comparisons, 64 seeded environment trials plus denial/collision cases). After installation and moving the prefix, the same test linked against the installed headers/archive and passed; installed source SHA-256 `92457cf18bdaa4fef2c713615bbfe526e55f0152b51ee6cdba346724d89165b4`. Only the access operation has a loader integration handoff. Environment operations are executable library operations, not a migration of the loader's entire environment or a separate-compilation ABI. Resolver, signature, seal, dependency and functor judgments are not implemented by this OML profile.
