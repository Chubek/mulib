# sshclient — SSH-2 identification and initial packets

Define `MULIB_IMPL_ENDIAN` and `MULIB_IMPL_SSHCLIENT`.
See [`examples/modules/sshclient.c`](../examples/modules/sshclient.c).

`identification` writes `SSH-2.0-software\r\n` with a printable, nonspace ASCII
software identifier of 1..244 bytes. `identify` parses one SSH-2.0 line of at
most 255 bytes, requiring CRLF, and copies software plus optional comments to
a NUL-terminated buffer. It does not skip pre-identification server banners
or accept SSH-1.x. Return 1 complete, 0 incomplete, -1 malformed/capacity.

`packet_encode` and `packet_decode` implement the unencrypted binary packet
format used before key activation: uint32 packet length, padding length,
payload, random padding. Total length is divisible by eight, padding is at
least four bytes, and payload is 1..32768 bytes. Encoding requires an explicit
RNG callback that fills padding and returns 1. Production callers must use
appropriate randomness. Decode borrows payload; it validates frame lengths,
padding bounds and alignment, returning 1/0/-1. `used=0` on failure.

All buffers are caller-owned and nonoverlapping for encoding. There is no key
exchange, algorithm negotiation, host-key verification, encryption/MAC, channel
management or socket connection. Encrypted SSH packets must be processed by
an SSH backend rather than these pre-key codecs. The server module supplies
separate user-authentication payload handling. Tests:
`tests/test_protocol_extensions.c`.
