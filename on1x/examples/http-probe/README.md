# Offline HTTP Probe

Run two independent probes through the standard-library API. The first compiles
an ElviScript document profile and extracts its title with a named On1x
procedure. The second supplies provider settings and a URL without a script,
uses a capturing closure to compute an `i64`, and delivers the result and
metadata to a typed handler. Both normalize HTML via `std.html`. The first
retains raw and normalized artifacts; the second withholds both optional
artifacts but still returns the canonical document.

`fixture.c` approves exactly two paths on reserved hostnames and supplies
in-memory HTTP replies in small chunks through the native Elvi transport. It
never opens a socket and its peer verification flags are synthetic. This
fixture is **not** a TLS policy or a sandbox. Do not reuse it as a live
network client.

## Build and run

Use a current On1x SDK and native Linux Clang. In a Clang-configured CMake build:

```sh
cmake --build build/on1x --target on1x_http_probe_example
build/on1x/on1x_http_probe_example
```

Or build from the checkout root using the explicit link plan:

```sh
on1xc=./build/on1x/on1xc
stdlib=./on1x/stdlib
runtime=./on1x/runtime
example=./on1x/examples/http-probe
kernel_headers=./kernels
work=./build/http-probe-example
mkdir -p "$work"

"$on1xc" --emit-c --stdlib "$stdlib" "$example/main.on1x" -o "$work/probe.c"
clang -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror \
  -I"$kernel_headers" -I"$runtime" -I"$stdlib" \
  "$work/probe.c" "$runtime/on1x_runtime.c" \
  "$stdlib/elvi/runtime.c" "$stdlib/elvi/bridge.c" \
  "$stdlib/elvi/capture.c" "$stdlib/elvi/live.c" "$stdlib/html/runtime.c" \
  "$example/fixture.c" "$example/kernels.c" -pthread -ldl -lm -o "$work/probe"
"$work/probe"
```

Expected output:

```text
fixture title: Daily & notes
manual fixture: Open studio
```

In an installed SDK, set `on1xc` to `PREFIX/bin/on1xc` and `stdlib`,
`runtime`, and `example` to the corresponding directories under
`PREFIX/share/on1x`. Use matching MuLIB `kernel_headers` from the checkout
or a separately provisioned headers tree. A restricted stdlib source bundle
can replace `stdlib`; it does not provide the compiler, runtime, example or
kernel headers.

`kernels.c` is the only MuLIB implementation translation unit in this example
and the composed fixture. Do not link any package's standalone `kernels.c`
or `lifecycle_kernels.c` into the same executable. The fixture references the
mandatory Elvi authorizer; the standard library does not provide a permissive
default.

Run `ctest --test-dir build/on1x -R '^on1x\.stdlib\.http_probe$' --output-on-failure`
to also execute the same application in a supervised child, with a durable
Kron recurrence reopened from disk, current authorization checked at each
occurrence, and deterministic cancellation by logical signal. Application
closures and TLS policy stay inside the process that owns them.
