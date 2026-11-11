# tls — native authenticated TLS 1.3 client

**Header:** `include/mulib-tls.h` · **Switch:** `MULIB_IMPL_TLS`

MuLIB's TLS client implements its cryptography, certificate verification and
wire protocol using MuLIB headers and C/POSIX APIs. It implements the TSL adapter
used by [HTTPS](mulib-https.md) and is [MuW3's](mulib-muw3.md) default HTTPS
connector. The historical `mulib-tsl.h` remains the generic lifecycle interface.

## Build and use

In one implementation translation unit define `MULIB_IMPL_TLS`, `TSL`, `SOCKET`,
`URI`, `STRBUF`, `BUFFER`, `INET6`, `INET4`, `PARSEC`, `X509`, `SIGNATURE`, `DER`,
`BASE64`, `SHA`, `ENTROPY`, `HKDF`, `X25519`, and `AES_GCM`; short names stand for
`MULIB_IMPL_<NAME>`. Define `_POSIX_C_SOURCE=200809L` before system headers for
native hostname resolution. `detail/mulib-tls-implementation.h` is an optional
explicit switch bundle for TLS and its cryptographic dependencies; it does not
select TSL/socket/URI dependencies. Public headers do not enable switches.

[`examples/modules/tls.c`](../examples/modules/tls.c) shows direct adapter I/O;
[`examples/muw3-https.c`](../examples/muw3-https.c) shows complete HTTPS requests.

```sh
make muw3
build/muw3 https://example.com/
build/muw3 --cacert roots.pem https://localhost:8443/
build/muw3 --cacert roots.pem --weblet library/lmuw3/examples/request.lua https://localhost:8443/
```

`mulib_tls_connect(options,endpoint,&adapter)` matches `mulib_https_connect_fn`.
`options` may be NULL or point to `mulib_tls_options_t`; endpoint is a parsed
HTTPS URI. Output must be a zero-state TSL adapter. Return 1 means TCP and trust
setup succeeded, not that the TLS handshake is complete. Call
`mulib_tsl_handshake` and then `mulib_tsl_read/write`; always close with
`mulib_tsl_close`, including after errors. TSL makes handshake/I/O failure sticky.
The adapter owns its session and socket. No global mutable session state exists.

Options borrow a preloaded `trust` store, an optional `ca_file`, and a
`timeout_ms` value (zero means 30000). A supplied store takes priority. Otherwise
the explicit file or system bundle is loaded into an owned store, released after
the handshake. Borrowed trust must remain alive during the handshake. Explicit
file failure does not fall back to system trust. No verification bypass exists.

## Supported profile

* TLS 1.3 (`0x0304`) with `TLS_AES_128_GCM_SHA256` (`0x1301`).
* Ephemeral X25519 key exchange; fresh OS entropy for scalar, random and session ID.
* SHA-256 transcript and HKDF schedule; independent client/server handshake and
  application traffic keys; Finished verification before application access.
* RSA-PSS SHA-256/384/512 and ECDSA P-256/SHA-256 or P-384/SHA-384
  CertificateVerify. SHA-2 PKCS#1/PSS/ECDSA certificate-chain signatures.
* [X.509 chain/SAN verification](mulib-x509.md), system or explicit PEM trust,
  SNI for DNS names, and ALPN offering only `http/1.1`. Absence of ALPN is accepted;
  selection of a different protocol is rejected.
* Handshake messages fragmented/coalesced across records, compatibility CCS
  within the handshake, optional client-certificate requests answered with an
  empty Certificate, NewSessionTicket parsing/discarding and bidirectional KeyUpdate.
* Authenticated alerts, close_notify and binary application records. Socket EOF
  without close_notify is an error, including for a complete HTTP Content-Length.

This is a client profile. It does not implement a TLS server, TLS 1.2, alternative
cipher suites/groups, HelloRetryRequest, resumption/PSK, 0-RTT, certificate
compression, client credentials, post-handshake authentication, TLS exporters or
HTTP/2. Unsupported negotiations fail. The single offered group already includes
its key share; cookie-only HelloRetryRequest is also rejected.

## Bounds and lifecycle

TLS plaintext is capped at 16384 bytes and ciphertext at 16640. Each handshake
message is capped at 256 KiB, total initial handshake bytes at 1 MiB, chain at
10 certificates, and empty/control-record loops at fixed limits. Session storage
is heap-owned, approximately 280 KiB plus temporary certificate/trust storage.
Each traffic key permits fewer than 2^23 encrypted records before the next
operation fails; KeyUpdate resets the sequence under a newly derived key.
Sequence numbers never wrap. The client responds to requested KeyUpdates; it
does not proactively refresh a key nearing its usage limit.

Native TCP uses system DNS and nonblocking descriptors with blocking readiness
waits. The timeout is per connect address/socket wait, not an overall request
deadline; DNS follows system resolver policy. Reads/writes through this connector
do not return AGAIN. A read may return fewer bytes than requested, and writes
send at most one 16-KiB application record. Retired session key buffers are wiped.

## Verification

`make check-tls` runs RFC 7748/5869/4231 and NIST GCM vectors, public-signature
oracles, record/KeyUpdate tests and loopback HTTPS interoperability. The Python
stdlib TLS server and `openssl` executable are independent **test-only** oracles;
runtime code links neither OpenSSL nor another third-party TLS library.
The tests cover RSA/P-256/P-384, intermediate chains, fragmented network writes,
optional client authentication, trust/hostname/time/EKU/path failures, tampering,
TLS-version rejection and truncated shutdown. This implementation has not had an
independent cryptographic audit.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-tls.h`](../include/mulib-tls.h) |
| Implementation switch | `MULIB_IMPL_TLS` |
| Repository-local includes | [mulib-aes-gcm](mulib-aes-gcm.md), [mulib-hkdf](mulib-hkdf.md), [mulib-socket](mulib-socket.md), [mulib-tsl](mulib-tsl.md), [mulib-uri](mulib-uri.md), [mulib-x25519](mulib-x25519.md), [mulib-x509](mulib-x509.md) |
| Complete example | [`examples/modules/tls.c`](../examples/modules/tls.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tls.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
