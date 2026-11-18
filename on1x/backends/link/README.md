# Yellowstone backend registration

On1x builds the built-in C++ backends as separate static libraries. With
`ON1X_USE_YELLOWSTONE_LINK=ON`, a build-host program calls the real LibStone
Yellowstone `qld_backend_manifest` API on [`backends.lua`](backends.lua).
On1x validates the exact built-in backend/registrar pairs, then generates the
`BuiltinRegistrars.inc` consumed by `Builtin.cpp`. That translation unit
contains the symbol references the target linker uses to extract the adapter
objects. Invalid manifests fail the build instead of falling back. The
option defaults to ON for a native build with a compatible LibStone source
checkout or when `ON1X_YELLOWSTONE_HOST_GENERATOR` is supplied. Other builds
default to the checked-in registrar map. Set `-DON1X_USE_YELLOWSTONE_LINK=OFF`
to force that portable fallback. Existing CMake caches retain their setting.

This is build-time selection of registration references, not a native
Yellowstone link of the driver or the C++ archives. The target C++ toolchain
still compiles and links the driver and adapters; Lua does not run inside
`on1xc` or compiled On1x programs. The manifest API does not depend on the
target object format, linker script dialect, or ELF/COFF/Mach-O support.

With a LibStone checkout that includes the manifest API:

```sh
cmake -S on1x -B build/on1x-yellowstone -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_YELLOWSTONE_LINK=ON
cmake --build build/on1x-yellowstone --target on1xc --parallel 4
ctest --test-dir build/on1x-yellowstone -R 'on1x\.backends\.yellowstone' --output-on-failure
```

Point `ON1X_LIBSTONE_ROOT` at an alternate source checkout if needed, or add
the installed Yellowstone SDK prefix to `CMAKE_PREFIX_PATH` and select its
`Yellowstone_DIR` if the source checkout also provides the API. An older
checkout/package without the manifest API does not support the opt-in mode.

For cross builds, first build the generator natively:

```sh
cmake -S on1x/backends/link -B build/on1x-manifest-host
cmake --build build/on1x-manifest-host --config Release
```

Configure target On1x with `-DON1X_USE_YELLOWSTONE_LINK=ON` and
`-DON1X_YELLOWSTONE_HOST_GENERATOR=/absolute/path/to/on1x-backend-manifest`.
Use the actual path for your native build configuration (including `.exe` on
Windows). Without a native generator, the opt-in is rejected when cross
compiling; the default static build remains available.
