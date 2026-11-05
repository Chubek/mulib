# mulib-aes

Original scalar FIPS 197 AES-256 encryption, 96-bit-nonce CTR, and authenticated
encrypt-then-HMAC-SHA256 messages. No external crypto library, entropy source,
password handling, padding, ECB message mode or implicit key management.
Define `MULIB_IMPL_AES` and `MULIB_IMPL_AUTHCRYPTO` in one C11 translation unit.
See `examples/modules/aes.c` and `tests/test_mumq.c` for usage and known-answer
tests (FIPS block and zero-key CTR vectors).

`mulib_aes256_block` encrypts one block. `mulib_aes256_ctr` encrypts/decrypts
bytes using nonce || big-endian 32-bit block counter, starting at one. The
maximum message is `(2^32-1)*16` bytes; counters cannot wrap within a message.
CTR alone is malleable: use seal/open for messages. Partial final blocks and
zero length are supported. Exact in-place operation is supported, arbitrary
partial overlap is not. Invalid arguments return zero, otherwise one.

`mulib_aes_seal/open` take a **64-byte secret key**: independently generated
AES key in the first 32 bytes and HMAC key in the last 32. The 32-byte tag is
HMAC-SHA256 over `nonce[12] || aad_length_be64 || aad || ciphertext`. AAD is
authenticated but not encrypted. Open checks the whole tag before decrypting;
authentication/allocation failures leave plaintext unchanged. Seal allocates
its temporary MAC buffer before changing output. NULL data/output is valid
only at length zero. All other outputs, keys, nonces, tags and AAD must be
disjoint, except exact input/output aliasing. MAC-buffer allocation uses checked
size arithmetic and is freed/wiped on exit.

**Never repeat a nonce under the same AES key**, including across restarts,
machines and directions. Provision independent random keys and durable unique
nonce namespaces outside this module. A password is not a key. The library
does not manufacture pseudo-random nonces or silently fall back to plaintext.
The implementation and best-effort volatile wiping have not been independently
audited; no constant-time, side-channel-resistance or production-security
certification is claimed. Review cryptography and key distribution before use
on an untrusted network.
