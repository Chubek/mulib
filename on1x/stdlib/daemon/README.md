# `std.daemon`: single-byte local IPC dispatch

This package implements one request to a handler **in the calling process**.
It is not a daemon launcher or an IPC connection to a running daemon.

```on1x
import std.daemon as Daemon;

fn main() {
  let reply = Daemon.basis_daemon_ipc_dispatch(0u8, 1000, fn(byte: i64) -> i64 {
    return byte + 42;
  });
  match reply with {
    Ok(value) => println(value),
    Err(status) => println(status)
  };
}
```

`basis_daemon_ipc_dispatch(request: u8, timeout_ms: i64, handler: fn(i64) -> i64)`
returns the framed adapter's nominal `Result<i64, i64>`. An invalid timeout
outside `1..1000` ms returns `Err(-1)` before opening a socket. Otherwise the
function opens a private `AF_UNIX` stream pair, sends one binary byte (including
NUL), and receives it through MuLIB framing under one monotonic deadline.
Only on success does it invoke the handler, exactly once on the caller's thread,
with the received byte widened to `i64` (`0..255`). Its result is an `Ok`, even
if it is negative. Opening and transport failures return the unchanged
[adapter status](../../docs/kernel-framed.md#one-shot-pair) and skip the handler.

Only the dispatcher owns the pair. The adapter consumes the owner and closes
both descriptors before calling the handler, including when the handler then
aborts the program. It retains no handler, and no socket or owner escapes.
There is no retry. The deadline bounds the framed transfer, not socket setup
or handler execution.

## Admission and limits

The implementation explicitly uses `unsafe import kernel mulib.ipc_framed`
with local `unsafe` blocks and the existing native Linux x86-64 GNU C11
KernelBind profile. Importing the wrapper does not turn the raw profile into a
reviewed safe kernel import. Safe `import kernel mulib.ipc_framed` still fails.
Use the static KernelBind build route below; this package requires neither
dynamic kernel compilation nor an additional C shim. It exposes no reusable
session, external peer, path, listener, or network connection.

The CMake installation includes these sources. The restricted `on1x-project.py`
standard-library source package does not: it does not support KernelBind
admission or linking.

MuLIB's [`daemon`](../../../manual/mulib-daemon.md) and
[`process`](../../../manual/mulib-process.md) kernels exist in
[`MANIFEST.yaml`](../../../MANIFEST.yaml), but On1x KernelBind has **no callable
profile** for either. The frontend rejects safe requests with
`ON1X-KERNEL-NOT-ADMITTED` and unsafe requests with
`ON1X-KERNEL-UNSAFE-UNSUPPORTED`. C declaration discovery alone cannot admit
them. Launching, stopping, waiting/reaping, restart and supervision, and
daemon configuration are not implemented here. A process-backed implementation
requires a separately admitted ABI profile for daemon specs and owned process
handles, including explicit stop/reap and failure-cleanup semantics.

## Build and test

Use `on1xc`, [Lua-enabled Exotract](../../docs/kernel-bind.md#inspect-requests-and-discover-declarations),
Clang resource headers, and a native C compiler. From the repository root,
point `sdk` at an existing SDK build, and choose fresh output paths:

```sh
sdk=build/kernel-shared-general
exo="$sdk/libstone-moonstone/exotract/exotract"
python3 on1x/tools/on1x-build.py on1x/stdlib/daemon/fixtures/dispatch.on1x \
  --kernel-bind --no-prelude --on1xc "$sdk/on1xc" --exotract "$exo" \
  --cc cc --resource-dir "$(clang -print-resource-dir)" \
  --mulib-plan "$sdk/daemon-ipc-link.json" -o "$sdk/daemon-ipc"
"$sdk/daemon-ipc"

python3 on1x/stdlib/daemon/test_daemon_ipc.py \
  --on1xc "$sdk/on1xc" --exotract "$exo" --cc cc
```

The fixture prints `0`, `255`, and `260` on separate lines. The test also
checks invalid timeouts, descriptor exhaustion, repeated cleanup under a
descriptor limit, the one-translation-unit MuLIB link plan, and rejection
without the required KernelBind mode. The existing `on1x.kernel_ipc_sdk` test
covers injected allocation and transport failures in the underlying adapter.
