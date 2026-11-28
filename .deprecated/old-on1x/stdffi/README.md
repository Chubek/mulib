# Standard MuLIB FFI

The [FFIHandler generator](../hir/FFIHandler/README.md) discovers C headers
through `MULIB_KERNELS_PATH` (default: the checkout's `kernels/` directory).
The build writes `.ffi` files under `stdffi/mulib/` in the build tree. Only
headers with representable, verified exports produce executable recipes.
Catalogue files record inspection results and cannot be imported as code.

`MULIB_FOREIGNLIB_PATH` selects generated recipe directories when compiling
`import foreign stdffi.<kernel>`. This variable never runs the generator or
changes the exports already validated at compiler build time.

The notes in [`overrides/mulib/`](overrides/mulib/) document ownership,
lifetimes, callbacks, and failure conditions.
