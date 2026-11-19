# Linux notebook desktop overlay

The [manifest](../MANIFEST.yaml) registers 83 additive headers under
`profile: notebook-desktop`. They are bounded Linux/POSIX C11 building blocks,
not a finished desktop, GPU renderer, notebook execution sandbox, or complete
implementation of X11, Wayland, or Jupyter. The existing
[graphics overlay](graphics-overlay.md) remains a separate set of 217 headers.

## Build and validation

Include the individual `mulib-*.h` headers from `include/`. The desktop headers
are `static inline`; do not define a desktop-specific `MULIB_IMPL_*` switch.
Legacy ABI dependencies of an application, such as the HMAC/JSON libraries,
still need their own switches in one translation unit. Use `-D_GNU_SOURCE`
before any system headers, `-std=c11`, and link `-lm -ldl` for the full profile.
GBM and xkbcommon are optional runtime libraries, not build dependencies.
Direct C++ inclusion is not part of this profile.

```sh
# A notebook model example, requiring no display or worker process:
cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/notebook-desktop-headers/consumer.c -lm -ldl -o notebook-example
./notebook-example

# Requires Python 3, GCC and Clang. No display server or graphics devices are
# needed. The original graphics gate is still separate.
make check-notebook-desktop
SANITIZE=1 make check-notebook-desktop
make check-graphics

# The same gate uses the configured compiler in a native Linux CMake build.
cmake -S . -B build/notebook-desktop -DCMAKE_C_COMPILER=clang
ctest --test-dir build/notebook-desktop \
  -R '^mulib_notebook_desktop_headers$' --output-on-failure
```

The desktop gate checks the manifest's paths and direct dependencies, compiles
each header alone and with repeated inclusion, then includes all 83 headers in
two independently compiled translation units and passes an owned notebook across
them. It also runs the [signed Jupyter envelope regression](../tests/test-jupyter-envelope.c)
and the [PTY regression](../tests/test-notebook-pty.c), which skips when `devpts` is unavailable.
`-Wmisleading-indentation` is enabled and treated as an error.
Use `python3 tests/run-notebook-desktop-headers.py --cc gcc` for a single
compiler (or repeat `--cc` for several). With the existing MuW3-enabled
CMake install, the headers are copied by the public include-directory rule.
Pass `--include PREFIX/include` to validate against installed copies rather
than the checkout. These are build and smoke checks, not per-API or protocol tests.

### Integration examples

| Example | Validation |
| --- | --- |
| [Notebook shell](../examples/notebook-headless.c) | `make check-notebook-headless`: CPU-rendered chrome and synthetic client surfaces, hit testing, PPM fixtures and notebook roundtrip. |
| [Private Wayland host](../examples/wl-headless-private.c) | `make check-wl-headless-private`: real private socket, SHM/xdg toplevel lifecycle and CPU pixels. No outer display or toolkit needed. |
| [Nested X11 window manager](../examples/x11-nested-wm.c) | `make check-x11-nested`: socketpair fixtures and, when available, live tests on a private Xvfb using `xauth`, `timeout` and XCB development files. |

The notebook and Wayland cases leave the active desktop untouched. Run the X11
example only against an explicitly selected private nested server.

For real client interoperability, run
`CC=clang SANITIZE=1 sh tests/run-wl-headless-private.sh`. It always builds the
wire fixture and private server CLI. The optional client uses `pkg-config`,
`wayland-scanner`, `timeout`, the `wayland-client` development files and
`wayland-protocols` (stable xdg-shell XML). It checks CLI PPM output and private
runtime directory cleanup after SIGTERM. Set `LIBWAYLAND=0` to skip that part
explicitly. To select a non-default Xvfb binary for `make check-x11-nested`,
set `XVFB` to its path.

## Notebook model

`nb-core` owns an ordered vector of cells; IDs are document-local and remain
stable across reordering. Accessors borrow pointers until mutation. Rebuild the
layout index after structural changes or group collapse; virtualized ranges may
contain zero-extent hidden cells. Scrolling moves chrome and scene nodes, not
the size of native windows. A notebook cell's focus is not permission to send
native keyboard events.

MNB1 stores core cell data and relationships, but not trust, runtime handles,
outputs or metadata. The notebook text model uses the upstream UTF-8 kernel.
`nb-ipynb` exports text, code and raw cells; it does not import notebooks or
export output bundles. `nb-store` syncs the temporary file and renames it in the
same directory; it does not sync the parent directory. History is bounded by
snapshot count, not bytes. Never execute code just because a notebook was loaded
or restored.

## Window policy

The `wm-*` registry, focus, placement, layout, and session modules model window
policy independently of the native connection. Notebook/window associations
are currently suitable for one active document. An acknowledgement alone does
not establish that a Wayland client has committed a matching buffer. The host
must validate native activation and focus requests and reconcile document
history with live window associations.

## CPU scene

The added `glx-*` headers compose straight ARGB8888 views on a caller-owned
surface using nearest-neighbour scaling and reference-counted buffer lifetimes.
Keep live client buffers at stable addresses. The render target may not alias
input buffers. The reference path walks the scene on each frame; the damage
and occlusion helpers do not turn it into a GPU or present backend. Use only
verified opaque regions for culling. The host must also filter transformed hits
using committed input regions before dispatching native input.

## X11 requests

The added `x11-*` headers serialize little-endian requests for a caller-provided
transport (`mulib_x11_wm_io`) and parse selected replies. The transport must
correlate replies to requests, preserve unrelated events and errors, and
bound allocation before reading a reply body. Helpers cover selected properties,
ICCCM/EWMH, grabs and extension requests; they do not by themselves manage a
server, implement full ICCCM/EWMH, or supply a compositing manager.

## Wayland server

The `wl-*` headers provide Unix socket/object plumbing, a core SHM surface path,
and a minimal version-1 xdg-shell toplevel handshake. SHM commits snapshot
pixels, so protocol buffer release can follow the copy while scene refs may
remain. Frame callbacks are not buffer-release or presentation fences. Seat,
output, data-device, layer, subsurface and DMA-BUF helpers do not constitute
complete protocol globals. XWayland supervision by itself is not rootless
integration. Keep validation private to a trusted runtime directory; do not
expose the reference server as a public compositor.

## Linux platform

The platform modules cover anonymous file backing, owned DMA-BUF descriptors,
runtime GBM/xkbcommon adapters, a seat broker interface and POSIX PTYs. They do
not acquire session ownership or bypass an active display server. Only use a
seat broker with authorized device access. A GBM export with unspecified
modifier is not evidence of linear layout.

## Execution

The execution modules are host-driven registries, queues, revision checks,
dependency graphs and worker primitives. The worker launcher is **not a
sandbox**; run trusted executables only and require explicit user authorization.
`jupyter-wire` can parse a bounded multipart envelope, verify or sign its four
JSON frames with MuLIB HMAC-SHA256, and decode bounded JSON objects after
verification. It does not validate Jupyter message schemas, implement a
ZeroMQ transport, authenticate a session, or protect trailing binary buffers
with that signature. Unsigned mode must be separately authorized.
