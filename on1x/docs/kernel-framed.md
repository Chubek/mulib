# KernelBind: raw framed local transport

On native Linux x86-64, `unsafe import kernel mulib.ipc_framed as Framed;`
projects **adapter** operations from the existing MuLIB framing kernel.
The adapter does not expose the C functions or a socket descriptor to On1x.
The profile is **unsafe-only**: a safe import fails, even in
review staging or alongside an unsafe alias. It exposes neither the operations
nor their types through the safe alias. The shipped sidecar remains unreviewed.

## One-shot pair

| Operation (inside `unsafe { ... }`) | Contract |
|---|---|
| `Framed.open_pair()` | Opens a connected, close-on-exec `AF_UNIX` stream pair, with at most 128 live pairs per executable. Returns an `Ok` with the opaque unique owner or `Err(i64)` with no live descriptors from that attempt. |
| `Framed.roundtrip(move(pair), byte, timeout_ms, callback)` | Consumes the owner on **every** outcome. Sends one framed `u8` and receives it through the peer with one absolute monotonic deadline. On success, closes both descriptors, then calls `(i64) -> i64` once with the received byte in `0..255`. Returns its value in `Framed.Result<i64,i64>.Ok`. On failure it closes the pair, skips the callback, and returns `Err(i64)`. |

`timeout_ms` must be in `1..1000`. Invalid bounds return `-1`; a system
failure returns `-2`, a pair or frame limit failure `-5`, expiry `-6`, protocol
failure `-7`, a box allocation failure `-8` and peer EOF `-10`. Kernel
failure codes pass through; none are delivered as callback values. Negative
callback return values remain ordinary successful values. There is no replay
or retry after a failed transfer. NUL is transported as the byte `0u8`, not
as a C string.

The pair has an inferred `own<T>` with a private, unspellable payload type;
use nominal `Result` `match` or `?` to extract it. The existing checked
owner rules govern `move`, `drop`, and cleanup on ordinary scope exit or
early return. It cannot be copied, `take`n, borrowed, made from an integer,
or captured by a closure. The `unsafe` block authorizes the foreign call,
not an ownership or ABI bypass. A returned owning `Result` can leave its
unsafe expression as an ordinary move-only value; this does not export a
loan or permit further raw calls outside `unsafe`.

The callback may be a named On1x function or a closure with captures allowed
by the existing language. The adapter calls its exact private C11 invoke
pointer with the generated closure environment, on the calling thread. The
environment stays rooted for the call; neither pointer is retained. The
adapter clears the transferred owner slot and closes the pair before entering
On1x, so even a terminal On1x assertion in the callback cannot leave this
pair open. The deadline covers transport only, not user callback code. This
is not a general unwind, callback registry or event-loop facility.

## Reusable session

[`framed_session.on1x`](../examples/kernel/framed_session.on1x) transfers a
registered session out of a helper, then polls it after the callback's original
scope has ended. Its public type is `own<Framed.Session>`. The `Session`
nominal has no On1x value representation: it cannot be constructed, boxed,
unpacked, or turned into an ordinary lexical `borrow` alias. Different import
aliases name the same nominal; a one-shot pair is not a session.

| Operation (inside `unsafe { ... }`) | Contract |
|---|---|
| `Framed.open_session()` | Opens a new `AF_UNIX` stream pair with both descriptors close-on-exec; returns `Framed.Result<own<Framed.Session>, i64>`. Shares the 128-live-owner quota with one-shot pairs. |
| `Framed.register_callback(session, callback)` | Retains exactly one `(i64) -> i64` closure on this owner. Duplicate registration returns `Err(-9)` without retaining the argument or replacing the callback. A finishing/closed session cannot accept a new registration. |
| `Framed.revoke(session)` | Releases the current callback, if any. If a frame is pending, its delivery is canceled, even if a new callback is registered before polling. Revocation on an open session is idempotent. |
| `Framed.send(session, byte, timeout_ms)` | Writes one `u8` through the real MuLIB framing kernel. Requires a callback and an open write side. Returns `Err(1)` without writing if a frame is already pending. |
| `Framed.poll(session, timeout_ms)` | With no frame pending and writing still open, returns `Err(1)` immediately. Otherwise reads the frame, returning `Err(-4)` if revoked or calling the retained closure once on the opening thread and returning `Ok(callback_result)`. After finishing and draining any pending frame, polling observes `Err(-10)` (EOF), closes both descriptors, and releases the callback. |
| `Framed.finish_send(session)` | Half-closes the actual writer with `shutdown(SHUT_WR)`. Idempotent before terminal close. Does not discard the pending frame or invoke the callback. |

All operations except opening return `Framed.Result<i64, i64>`; successful
non-poll operations return `Ok(0)`. Negative callback values are still `Ok`.
`send` and `poll` each take a deadline in `1..1000` ms; an invalid argument
returns `Err(-1)` without changing the owner. After a valid transport operation
fails (including timeout and invalid framing), both descriptors close and the
registration is released. Later operations on that owner return `Err(-9)`;
the inert box still counts against the quota until dropped. Dropping an owner
directly or by scope exit also closes both descriptors and releases its
callback, without dispatch, even if a frame is pending. The deadline bounds
transport, not callback execution. The registration quota bounds retained
roots, not the size of user-created capture graphs. Terminal On1x assertions
still exit the process, not unwind through C; this is not a recoverable
callback-panic ABI.

Use a named session local or by-value parameter directly as the first
argument, not `move(session)`. The checker holds an exclusive call-scoped
loan through later arguments and the native call. It rejects a conflicting
read, move, drop, or nested operation in those arguments. Closures cannot
capture the owner or a lexical loan. The runtime also rejects operations on
the same owner while its callback is active. No OS or worker thread invokes
the closure: only `poll` does, synchronously, with the owner still borrowed.
The private adapter does not extend the general closure/owner runtime to
cross-thread transfer, concurrent destruction, or embedded foreign use.

## Build and evidence

Use the [Lua-enabled Exotract SDK](kernel-bind.md#inspect-requests-and-discover-declarations).
From the repository root, with fresh output paths:

```sh
sdk=build/kernelbind-gcc
python3 on1x/tools/on1x-build.py on1x/examples/kernel/framed_pair.on1x \
  --kernel-bind --no-prelude --on1xc "$sdk/on1xc" \
  --exotract "$sdk/libstone-moonstone/exotract/exotract" \
  --cc gcc --resource-dir "$(clang -print-resource-dir)" \
  --mulib-plan "$sdk/framed-link.json" -o "$sdk/framed"
"$sdk/framed"
python3 on1x/tools/on1x-build.py on1x/examples/kernel/framed_session.on1x \
  --kernel-bind --no-prelude --on1xc "$sdk/on1xc" \
  --exotract "$sdk/libstone-moonstone/exotract/exotract" \
  --cc gcc --resource-dir "$(clang -print-resource-dir)" \
  --mulib-plan "$sdk/framed-session-link.json" -o "$sdk/framed-session"
"$sdk/framed-session"
ctest --test-dir "$sdk" -R '^on1x.kernel_ipc_sdk$' --output-on-failure
```

The one-shot example prints `0`, `42`, then `-1` on separate lines. The
session example prints `42` and `297`, asserts bounded backpressure,
revocation before replacement, half-close, drain, and terminal EOF.

The driver validates the canonical ExoLib/CDecl target, provenance, calling
convention, qualifiers, integer widths, pointer arguments and framing record
layout. C11 assertions independently check the actual declarations and layout
used by the native compiler. The planner compiles both implementation switches
(`MULIB_IMPL_SANDBOX_PROFILE`, `MULIB_IMPL_IPC_FRAMED`) in one TU. Only the
approved build flags and canonical `kernels/` directory are used. The optional link
plan pins the application, tools, adapter, kernel inputs and executable
and reports `safe_export:false`; its explicit session loan and callback
descriptions are adapter policy, not reviewer evidence. `--inspect-kernels` still reports the
canonical C symbols as unadmitted; `--emit-c --kernel-c11` alone just emits
an unresolved generated caller. Neither report is a reusable authorization.
This resource ABI is static and requires `--kernel-bind`; it is not available
with `kernel[dynlib]`. Other resource
declarations do not become callable just by changing the spelling of the import.

`on1x.kernel_ipc_hir` also corrupts the checked ABI shape and ownership/loan
metadata to verify rejection before emission. `on1x.kernel_ipc_sdk` runs both
examples plus cross-module retention and cleanup, and links a separate C++17
client against the C11 runtime and framing implementation. The client checks
all byte values against the actual wire format; injected socket, initialization,
allocation, clock, malformed-frame and shutdown failures; wrong-thread and
reentrant dispatch rejection; and callback/root/descriptor cleanup.

This profile cannot be mixed with other callable kernel profiles in the build
driver; separate source modules may import it under their own aliases. It
admits no user C, arbitrary endpoints, network access, path-based sockets,
descriptor passing, multi-byte frames, shared endpoints or escaping loans,
arbitrary or foreign-thread callbacks, task groups, typed ownership MIR, SDK
foreign-resource transport, or other targets. It installs no sandbox
policies and grants no process or network authority. Extending it to safe
imports still requires independent semantic review and usage verification.
