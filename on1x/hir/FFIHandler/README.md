# Standard FFI recipe compiler

LLSynth parses `.ffi` inputs at build time. The compiler validates their
closed `ffispec 1` profile and generates immutable typed tables. MIR lowering
materializes only referenced exports; it needs no `.ffi` reads, C symbol
lookups, Python or Perl at runtime. The separate native C11 runtime resolves
its typed entry points in a module compiled by Cephyr on first use.

```text
(ffispec 1
  (kernel clamp)
  (header mulib-clamp.h)
  (sha256 HEADER_DIGEST)
  (export i64 mulib_clamp_i64
    (params x lo hi)
    (guard (le lo hi))
    (body (if (lt x lo) lo (if (gt x hi) hi x)))
    (hint private-code (backend mir-c11))
    (hint kernel-hot (backend mir-c11) (target x86_64-unknown-linux-gnu) (speculative))
    (rewrite fold-identity (speculative))))
```

The `backend` and `target` selectors can also be specified directly within
the spec or export. Directives inherit selectors from those scopes; a
nested selector overrides the one it inherits. Omission at every level,
or an explicit `*`, produces the empty (unrestricted) MIR selector.
Either directive kind may carry the argument-free `(speculative)` marker.
Without the marker a directive is non-speculative.
Directives carry no authority over ABI, guards, types or effects.

The compiler checks the actual MuLIB header hash against `reviewed.json`,
independently of the hash claimed by `.ffi`. It also checks every typed
guard and body against the independently recorded structural identity in
that review, comparing parameters by position. Adding or changing a body
requires a new review of both its C implementation and recipe, not just a
new header hash. For `clamp`, the interval guard runs before the body;
the saturation recipes use short-circuit sign tests to make the
subsequent arithmetic safe before each bound check.

When Perl or LibStone LLSynth is unavailable, the build uses a checked-in
compiled bootstrap. The build will reject it if its payload, compiler,
grammar, recipe source, review lock, or a reviewed header has changed. Rebuild the snapshot
with `cmake --build <build-dir> --target on1x_stdffi_regenerate` in a build
configured with real LLSynth. The generated and bootstrap tables are checked
for byte-for-byte parity by `on1x.stdffi.bootstrap`.
