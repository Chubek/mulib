# Framed IPC — discovery-only: `private_companion_resource_lowering`

Authored semantic record, not a declaration, descriptor or trust grant. Keep
the existing unsafe-only bridge until the generic adapter passes ownership,
callback, scheduler and native admission gates.

## Two different interfaces

- MANIFEST `ipc-framed` is `kernels/mulib-ipc-framed.h`, depending on
  `sandbox-profile`. The real native recipe builds the two implementation
  macros in one TU. The public framing record **borrows** its connected
  stream FD, requires exclusive access and absolute monotonic deadlines;
  its caller must close on poison/failure. Receive can partly overwrite the
  destination on failure; it only publishes length on success. No retry or
  replay after partial I/O. The monotonic clock is nondeterministic. These
  public C declarations are discovery-only; there is no direct call with a
  raw FD or by-value framing record.
- The eight `on1x_kernel_ipc_v1_*` declarations are in the **private**
  `on1x/runtime/on1x_kernel_ipc.h`, with `ON1X_KERNEL_ABI_V1=1`, Linux AMD64
  LP64 and existing closure/unique ABI v1. The closed companion recipe uses
  that header, `on1x_kernel_ipc.c`, the existing runtime and the canonical
  MANIFEST closure. It is not a public header alias. Cephyr projects both
  headers, but only the companion declares these entry points.

## Companion semantic delta

Qualified result identity is `mulib.ipc_framed.Result`; status `0` alone
selects `Ok`, everything else (including retry-later status `1`) selects
`Err(i64)`. Negative callback *results* are ordinary `Ok` values. Indices below
are zero-based native positions; exact types must come from the companion.

| Companion suffix | Ownership, loan, callback and output obligations |
| --- | --- |
| `open_pair` | Position 0 accepts a fresh out-slot; only status `0` creates sealed `own<Pair>`. No FD or box on failure. |
| `roundtrip` | Position 0 consumes the live `Pair` on valid transfer, even when the timeout or transport fails. Positions 1/2 are exact U8 and i64 timeout (1–1000 ms). Positions 3/4 form one synchronous, non-retained closure with the exact invoke ABI. Both descriptors close and the transfer is invalidated before dispatch. Position 5 is written only on `0`. |
| `open_session` | Position 0 accepts a fresh out-slot; only `0` creates sealed `own<Session>`, distinct from `Pair`. Shares the 128 live-box quota with pairs. |
| `register_callback` | Position 0 is an exclusive call loan. Positions 1/2 hold a closure whose environment is retained only on `0`. Reject duplicate/finishing/closed registration; do not replace an existing root. |
| `revoke` | Position 0 is an exclusive call loan. Same-thread revocation releases the current registration and cancels dispatch of already pending data, even if a new registration is made before polling. Idempotent on an open session. |
| `send` | Position 0 is an exclusive call loan, positions 1/2 are U8 and timeout. Requires a registration and open write side. Allows one pending frame; `Err(1)` writes nothing. |
| `poll` | Position 0 is an exclusive call loan, position 1 is a timeout; no writing to position 2 on error. Only the opening-thread poll invokes the retained callback, synchronously and at most once for each accepted frame. EOF after half-close and drain is terminal, not `Ok`. |
| `finish_send` | Position 0 is an exclusive call loan. Half-close without dropping pending delivery; idempotent before terminal close. |

Successful session operations without an output become `Ok(0)`. Invalid
session timeout leaves state unchanged. Terminal I/O/EOF closes the pair and
releases the registration, but the inert box still counts against the quota
until dropped. Ordinary scope exit and early return use the existing unique
destructor; neither closing nor destruction invokes a registered callback.
The one-shot deadline covers transport, not closure code.

Unresolved generic mechanics: acquire/move/drop of compiler-sealed nominal
owners across aliases; exclusive named-place loan begins **before** later
argument evaluation; reject conflicting access, nested call, lexical alias,
owner construction/take/capture, escape and use-after-move. No raw pointer
transport, cross-thread calls or destruction, task groups, scheduled/async
callbacks, deferred session calls, unsupported backends, or safe import.
Native ABI checks, target/environment, artifact identity, scheduler/mixing
policy and independent trust remain the responsibility of external admission.

Audit inputs: real public/private headers and implementations;
`on1x/contracts-exoml/sandbox/ipc-framed.exocontract.json`;
`on1x/docs/kernel-framed.md`. Do not retire the old path on the basis of
this record.
