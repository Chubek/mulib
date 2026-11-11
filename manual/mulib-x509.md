# x509 — server certificates and trust stores

**Header:** `include/mulib-x509.h` · **Switch:** `MULIB_IMPL_X509`

Dependencies: `SIGNATURE`, `DER`, `SHA`, `ENTROPY`, `BASE64`, `INET6`, `INET4`,
and `PARSEC`. [`examples/modules/x509.c`](../examples/modules/x509.c) has a
complete switch setup and a command-line verification example.

## Parsing and ownership

`mulib_x509_parse(der,length,out)` returns 1/0, preserving output on failure.
A certificate is at most 64 KiB. The result borrows DER: retain that storage
through all verification operations. Encoded TBS, issuer, subject, signature,
SAN and public-key slices are exposed, along with UTC Unix validity timestamps,
CA/path-length, key usage and server EKU information.

The parser checks the certificate structure, matching inner/outer signature
algorithms, positive serial, supported SPKI, UTC/GeneralizedTime syntax and
calendar values, duplicate extensions and the supported extension encodings.
It supports RSA 2048–4096 and P-256/P-384 public keys; certificate signatures
use the [signature module's](mulib-signature.md) SHA-2 schemes. RSA-PSS
parameters must explicitly select a supported hash and matching MGF1/digest
salt. SHA-1, EdDSA, P-521 and RSA-PSS-constrained public keys are unsupported.

## Identity and chain verification

`mulib_x509_hostname(cert,host)` uses SAN only; it never falls back to subject CN.
DNS comparisons are ASCII case-insensitive. Supply an ASCII IDNA A-label, not
Unicode. An entire leftmost `*` label matches exactly one nonempty label and
requires at least two suffix labels. Numeric IPv4/IPv6 hosts match binary IP
SANs, never DNS strings. Brackets and trailing dots are not accepted by this API;
the TLS connector removes URI IPv6 brackets. No public-suffix-list lookup occurs.

`mulib_x509_verify(chain,count,store,host,now)` requires 1..10 leaf-first
certificates. It verifies SAN, validity, signatures, CA basic constraints,
path-length limits, digitalSignature/keyCertSign usage when present, and server
EKU when present. A leaf marked CA is rejected. Issuer/subject matching compares
encoded DER names exactly. The supplied chain order is authoritative; it does
not search alternate intermediate paths or download missing issuers.

Each supplied certificate can terminate at a matching trusted certificate or
an anchor that verifies its signature. Time, EKU and CA/path constraints apply
to anchors too; anchor self-signatures need not be verified. Unknown critical
extensions and name/policy constraints that this profile cannot enforce cause
rejection. Unknown noncritical extensions are ignored. There is no OCSP/CRL,
certificate transparency, AIA retrieval or policy-tree processing.

## Trust stores

Zero-initialize `mulib_x509_store_t`. `mulib_x509_store_file(&store,path)` appends
supported `CERTIFICATE` PEM blocks atomically, owning their decoded DER. File
size is bounded at 16 MiB, each certificate at 64 KiB, total anchors at 512.
Unsupported certificates in mixed bundles are skipped; malformed PEM/base64 or
no supported certificates returns 0. Consequently this loader is for trust
bundles; use `parse` directly when every supplied certificate must be accepted.
Free stores with `mulib_x509_store_free`; it accepts NULL and repeated calls.

`mulib_x509_store_system` tries `/etc/ssl/certs/ca-certificates.crt`,
`/etc/pki/tls/certs/ca-bundle.crt`, `/etc/ssl/cert.pem`, then
`/etc/ssl/ca-bundle.pem`. It uses the first usable file. It does not read
environment overrides, hash directories or platform keychain databases.
An application may share an immutable loaded store across clients.

Tests use ephemeral independent RSA/ECDSA CAs, intermediate chains, wrong hosts,
untrusted issuers, expired certificates, EKU failures, critical extensions,
path constraints, wildcards and IP SANs. Run `make check-tls`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-x509.h`](../include/mulib-x509.h) |
| Implementation switch | `MULIB_IMPL_X509` |
| Repository-local includes | [mulib-base64](mulib-base64.md), [mulib-inet6](mulib-inet6.md), [mulib-signature](mulib-signature.md) |
| Complete example | [`examples/modules/x509.c`](../examples/modules/x509.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/x509.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
