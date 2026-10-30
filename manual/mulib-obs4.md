# obs4 — obfs4 backend adapter

Define `MULIB_IMPL_XTKS` and `MULIB_IMPL_OBS4`. The filename preserves `obs4`;
the intended protocol is **obfs4**. See
[`examples/modules/obs4.c`](../examples/modules/obs4.c).

Initialize a zeroed adapter with a copied 52-byte decoded certificate
(20-byte node ID plus 32-byte public key), a borrowed backend context and all
four operations. The backend is responsible for actual obfs4 handshake,
cryptography, certificate pinning, replay defenses, packet framing and padding.
No cryptographic implementation or alternative plaintext transport is supplied.

`handshake` returns 1 for resumable AGAIN, 0 only when the backend returns
success **and** authenticated=1, or a negative sticky failure. The adapter does
not permit application I/O before that transition. `stream` produces a borrowed
XTK stream. It checks callback progress/count/status contracts and makes I/O
errors sticky. The adapter must remain live and unmoved while that stream is
used. EOF is valid for reads; AGAIN transfers no bytes.

`close` invokes backend close once, clears adapter state and is idempotent.
Recursive handshake/read/write calls return EINVAL. Close requested during a
backend callback is deferred until it returns; the outer operation reports
EINVAL with zero count and leaves the adapter closed. Repeated close, including
from backend close itself, does not release the backend twice. Callbacks must
not mutate/reinitialize the adapter or free its storage.
Reinitialization requires closing the old session. Context ownership remains
with the application, subject to its backend close contract. The example uses
a rejecting fixture, not fake obfs4 encryption; tests in
`tests/test_transport_extensions.c` exercise state transitions and pinning failure.
