# Bounded graphics overlay

This additive overlay is C11, header-only `static inline` code. It has no
`MULIB_IMPL_*` switch. Existing externally linked MuLIB APIs and their switches
remain separate. `MANIFEST.yaml` marks the overlay as **bounded**, not as toolkit,
Unicode, wire-protocol, font, desktop-service, or GPU feature parity.

The `implemented_module_count` field continues to count legacy ABI modules.
`bounded_module_count` counts registered bounded modules, including the separate
[notebook desktop overlay](notebook-desktop.md). `module_count` counts all
registered modules; unrelated pre-existing unregistered headers are not promoted
or assigned a status by the graphics generator.
`python3 tools/graphics_inventory.py --check` validates the overlay inventory
and direct include dependency graph. After deliberately changing header
dependencies, refresh it with `--write`. Handwritten module records, including
the desktop profile's `glx-*` and `x11-*` headers, are preserved.

## Dependency order and scope

| Layer | Direct foundations | Contract and tests |
| --- | --- | --- |
| Raster/storage/draw lists | C11 allocation and math | [Raster](mulib-gfx-raster.md), `test-gfx-raster.c` |
| Paths, strokes and SVG subset | Shared raster | [Paths](mulib-gfx-path.md), `test-gfx-path.c` |
| Bitmap fonts and image decoding | Raster and UTF-8 buffer | [Fonts/images](mulib-gfx-font-image.md), `test-gfx-font-image.c` |
| Text selection, editing and history | Validated UTF-8 buffer | [Text](mulib-gfx-text.md), `test-gfx-text.c` |
| Presentation/windows | Surfaces, regions and explicit platform | [Presentation](mulib-gfx-present.md), `test-gfx-present.c` |
| Input/event loop | Synthetic events, caller-owned FDs | [Input](mulib-gfx-input.md), `test-gfx-input.c` |
| Native ImmGTK | Shared GUI, draw lists, text, input | [ImmGTK](mulib-gfx-immgtk.md), `test-gfx-immgtk.c` |
| Native RetGTK objects/events | Shared GUI and reference-counted widget tree | [Core](mulib-gfx-retgtk-core.md), `test-gfx-retgtk-core.c` |
| Native RetGTK models/layout | RetGTK core, shared text and painter | [Models/layout](mulib-gfx-retgtk-model-layout.md), `test-gfx-retgtk-model-layout.c` |
| Lua ImmGTK | Application native hooks | [Lua ImmGTK](mulib-gfx-immgtk-lua.md), `test-gfx-immgtk-lua.lua` |
| Shared GUI/Lua RetGTK | Shared GUI primitives, application hooks | [GUI](mulib-gfx-gui.md), `test-gfx-gui.c`, `test-gfx-retgtk-lua.lua` |
| DRM and native wire helpers | Real DRM UAPI or caller-owned protocol FDs | [DRM](mulib-gfx-drm.md), [Wire](mulib-gfx-wire.md), mocked/device-free tests |
| Scalar shader interpreter/folder | Checked arithmetic DAG, no executable memory | [Shader IR](mulib-gfx-shader.md), `test-gfx-shader.c` |

These are family-level tests, not evidence that every function or optional
backend has behavioral coverage. Exact supported subsets and remaining work
belong in the linked contract pages and `AGENTS.md`.

## Reproducible validation

```sh
make check-graphics
SANITIZE=1 make check-graphics
CFLAGS=-O2 make check-graphics
CFLAGS=-O2 SANITIZE=1 make check-graphics
```

The gate checks standalone and repeated header inclusion, combined C11 linking,
behavioral C suites, independent C translation units, a C++17 consumer of an
explicit C ABI wrapper, the headless demo, stock-Lua suites when available, and
the four existing ImmGTK/Mune/canvas/drawlist/plot regressions.
The Lua ImmGTK regression also runs through the actual Mune tree interpreter
and bytecode VM; its stub hooks still do not certify all native integrations.
Direct inclusion of all C11 overlay headers in C++ is **not** certified by the
C ABI consumer test. `LEGACY=0` skips the relatively expensive legacy suites.
`CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `LUA` and `TMPDIR` are configurable.
The compact formatting still present in untouched headers requires the narrow
`-Wno-misleading-indentation` exception; all other listed warnings are errors.

Run the example without any display server, device access, fonts, or GPU:

```sh
cc -std=c11 -Iinclude -Wno-misleading-indentation examples/gfx-headless.c -lm -ldl -o /tmp/opencode/gfx-demo
/tmp/opencode/gfx-demo /tmp/opencode/gfx-demo.ppm
```

It renders both toolkits through the same CPU surface and explicit headless
presentation contract. The optional output is a real P6 PPM image, not a
hardware-validation result.

### Validated checkout snapshot (2026-11-19)

- 217 overlay headers passed independent/repeated inclusion and combined C11
  linkage, including opt-in bridges against real upstream declarations.
- `make check-graphics` and `SANITIZE=1 make check-graphics` passed 18 C suites,
  independent C/C++ ABI consumers, real Mune bridge callbacks, Mune tree/VM Lua
  regressions, two stock-Lua suites, legacy widget/pixel tests, and the demo.
  Both gates also passed with `CFLAGS=-O2`; the final sanitizer/CMake rerun used
  an isolated build-directory `TMPDIR` after temporary files disappeared.
- All 117 overlay Lua files passed syntax checks. The 14 primary graphics C
  targets also built and passed through CMake/CTest.
- Nine tooling regressions passed. The existing compiler gate passed C++17
  linkage, 60 independent modules, eight regression suites, and 60 examples.
- This is scoped validation, not a repository-wide inventory certification:
  259 unrelated headers were already absent from the starting manifest. Their
  registration/status and the global manual index are deliberately not rewritten.
- A 32-bit compiler probe failed because the host lacks `gnu/stubs-32.h`.
  No 32-bit runtime, device, compositor, X server, or desktop-service validation
  is claimed.

### Graphics follow-up

The follow-up remains confined to the existing graphics/UI layer:

- Opt-in native, bounded noninterlaced 8-bit PNG decoding reuses the existing
  inflater and checksum kernels. The original PNG encoder and strict C++17
  header inclusion remain compatible; a C-producer/C++-consumer round trip
  runs in the graphics gate.
- Native ImmGTK windows support captured move/resize gestures, collapse,
  minimum dimensions and content clipping through `window_begin_ex`.
  Applications still control painter order and raising.
- RetGTK supports damage-driven repaint, immediate clipped scrolling and a
  retained backing image. Presentation still copies that full image.
- The optional Wayland client supports bounded registry/xdg bootstrap,
  configure/ack, host-supplied SHM buffers, release-safe resize and disconnect
  cleanup. Frame callbacks are pacing, not physical presentation feedback.

The expanded gate passed **217 headers and 22 C suites** with optimized GCC and
optimized Clang ASan/UBSan, including PNG C/C++ linkage, actual Mune bridge and
interpreter/VM regressions, legacy widgets, Lua suites and the headless demo.
No live compositor or hardware validation was performed. See the linked family
manuals for the supported subsets and remaining limitations.

## Acceptance gates still required

Mock ioctls and socketpairs do not substitute for VKMS, nested Wayland/Xvfb,
real seat/session ownership, desktop D-Bus services, protocol lifecycle tests,
IME, Unicode conformance data, or real GPU draws/fences. Optional runtime
loaders are not GPU drivers. Do not run direct DRM tests on an active desktop
or change device permissions to make tests pass. Headless tests never need
`/dev/dri` or `/dev/input`.
