# MuLIB SSH kernel handoff

If using the ZIP, extract it over the root of a MuLIB checkout. The headers target
the C11 kernel conventions in `Chubek/mulib` at commit `a85e67d`. The checkout
supplies `mulib-endian.h`, `mulib-entropy.h`, `mulib-x25519.h`, and
`mulib-aes-gcm.h`. Use OpenSSL 1.1.1 or newer with Ed25519 support and link
`libcrypto`. The initial implementation was tested with OpenSSL 3.5.7.

## Build and use

Put these switches in one C translation unit. Keep them out of declaration-only
callers. Legacy client and server callers can continue defining only their
original implementation switch and `MULIB_IMPL_ENDIAN`.

```c
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_X25519
#define MULIB_IMPL_ED25519
#define MULIB_IMPL_SSHWIRE
#define MULIB_IMPL_SSHPROTO
#define MULIB_IMPL_SSHPACKET
#define MULIB_IMPL_SSHCRYPTO
#define MULIB_IMPL_SSHKEY
#define MULIB_IMPL_SSHKEX
#define MULIB_IMPL_SSHTRANSPORT
#define MULIB_IMPL_SSHAUTH
#define MULIB_IMPL_SSHCHANNEL
#define MULIB_IMPL_SSHCLIENT
#define MULIB_IMPL_SSHSERVER
#include "mulib-sshclient.h"
#include "mulib-sshserver.h"
#include "mulib-sshchannel.h"
```

Compile that unit with `cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -c ssh-impl.c`.
Link it into the application with `-lcrypto`.

Exchange identification lines in the application and pass the exact lines,
excluding CRLF, to the role's start function. The client must acquire the
host public key pin from a trusted source. The server owns its persistent host
seed. Queue emitted frames in order, retain incomplete input, and pass each
complete record through the transport handshake or encrypted packet API.
An output batch can contain both ECDH_REPLY and NEWKEYS. Once the transport
reports ready, exchange the `ssh-userauth` service messages and authenticate.
Use the attempt-limited server policy with an application account verifier or
key authorizer. A public-key query can receive PK_OK after policy approval.
Only a valid signed request can authenticate the user. Scope that auth
state to the connection and complete auth before dispatching channel messages.

The suite is `curve25519-sha256`, `ssh-ed25519`,
`aes128-gcm@openssh.com` in both directions, and no compression.
Transport handles initial key exchange and bounded records. The application
owns socket and banner handling, short reads and writes, credential and host
key storage, user and channel authorization, channel IDs, and dispatch.
Channel request bodies are opaque. The application decides which requests
to accept and what action each request performs. Rekey and multi-factor
authentication are follow-up work. Close the transport on a fatal return or
when the AEAD packet or byte limit is reached. Clear the context at teardown.
Keep scratch buffers private and wipe credential-bearing plaintext after use.

## Tighten and verify

- Preserve the legacy identification, packet, and password-policy entry
  points and their return conventions. Preserve transactional cursor
  reads and channel state updates. Re-run the existing protocol test with
  `cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_protocol_extensions.c -lm -pthread -ldl -o /tmp/mulib-protocol-test`
  and execute it.
- Add a permanent focused integration test. Cover RFC 8032 Ed25519 and
  RFC 7748 X25519 vectors, then add independent RFC 4253 KDF and RFC 8731
  exchange-hash fixtures. Assert exact transcript treatment for CRLF,
  comments, raw KEXINIT payloads and positive mpint sign padding. Run
  ASan/UBSan and build the implementations in separate translation units.
- Fuzz packet, string, KEXINIT, key blob, auth, and channel parsers with
  fragmented and coalesced frames, boundary lengths, trailing bytes and
  invalid padding. Check wrong pins, invalid signatures, all-zero X25519
  results, length/tag tampering, replay, short output retries and AEAD
  exhaustion. Confirm fatal errors destroy keys and cannot release an
  unauthenticated payload.
- Add bidirectional OpenSSH integration tests with explicit pins and the
  negotiated suite. This delivery checked the OpenSSH 10.0 client against
  MuLIB's server over pipes, including Ed25519 authentication and a session
  channel carrying fixed output. It also checked both MuLIB roles together
  under ASan/UBSan. Add an OpenSSH server fixture for the reverse direction,
  then cover rejected authorization, extended data and close races.

## Expand and solidify

- Implement rekey as a transport state machine. Keep the first exchange
  hash as the session ID, install fresh keys at each direction's NEWKEYS
  boundary, and test interleaved records. Implement strict key exchange with
  its sequence number rules before advertising it. Exercise algorithm and
  extension negotiation independently before advertising extra algorithms.
- Add service and authentication sequencing, banner handling, and a
  connection driver that retains partial writes. Bind a server auth policy
  to its connection and test key and user changes across attempts. Make
  channel dispatch handle pending opens and replies, credit updates and
  both CLOSE messages before reusing a channel ID.
- When integrating upstream, register the new kernels in the MuLIB manifest,
  declaration and implementation inventories, and build targets. Keep
  provider-backed builds explicit. Update the two existing SSH manuals and
  add manuals and runnable examples for the new public APIs.
- If you add or find temporary handoff comments about expansion or
  solidification in the headers, remove those comments once the work they
  describe is implemented and verified. Keep enduring API contracts and
  protocol rationale.
