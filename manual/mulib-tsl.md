# tsl

Historical TSL spelling for an explicit **TLS backend adapter**, not encryption.
Zero initialize; `init` requires borrowed handshake/read/write/close callbacks
and context. Handshake returns OK only after actual hostname/chain verification
with verified=1. AGAIN permits retry; failed/unverified handshake is sticky and
prevents I/O. There is no plaintext fallback or built-in cryptographic backend.

Read/write require an authenticated state. Backend counts must fit capacity;
error/AGAIN/EOF must have zero count, and invalid results fail closed. Read may
report EOF, write only OK/AGAIN. Short success I/O is allowed. `close` invokes
backend close once and resets, even after failed handshake. Backend/context must
remain live until close and callbacks must not reenter adapter. This is not
peer verification by itself: security depends entirely on a correctly configured
real TLS backend. Test doubles only validate lifecycle/result handling.

Switch: `MULIB_IMPL_TSL`. Example: `examples/modules/tsl.c` deliberately uses an
unverified fake backend to demonstrate refusal of unauthenticated sessions.
