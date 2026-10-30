# tsl

Historical TSL spelling for an explicit **TLS backend adapter**, not encryption.
Zero initialize; `init` requires borrowed handshake/read/write/close callbacks
and context. Handshake returns OK only after actual hostname/chain verification
with verified=1. AGAIN permits retry; failed/unverified handshake is sticky and
prevents I/O. There is no plaintext fallback or built-in cryptographic backend.

Read/write require an authenticated state. Backend counts must fit capacity;
error/AGAIN/EOF must have zero count, and invalid results fail closed. Read may
report EOF, write only OK/AGAIN. Nonempty successful I/O must make positive
progress; empty I/O succeeds without invoking the backend after authentication.
`close` invokes backend close once and resets, even after failed handshake.
Recursive handshake/read/write calls return EINVAL. Close requested during a
backend callback is deferred until that callback returns; the outer operation
then returns EINVAL with zero count and leaves the adapter closed. Backend close
may itself call close without recursively releasing the session. Backend/context
must remain live until close; callbacks may not mutate/reinitialize the adapter
or free its storage. This is not peer verification by itself: security depends
entirely on a correctly configured real TLS backend. Test doubles only validate
lifecycle/result handling.

Switch: `MULIB_IMPL_TSL`. Example: `examples/modules/tsl.c` deliberately uses an
unverified fake backend to demonstrate refusal of unauthenticated sessions.
