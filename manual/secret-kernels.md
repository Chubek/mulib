# Secret kernels

The `mulib-secret-*` headers are a **development baseline for NyxG8**, not an
audited vault, password manager, or authenticated protocol. Use synthetic data
only. Manifest status `implemented` means the named C API exists, not that the
end-to-end feature is available or safe for production.

| Layer | Kernels | What the host still owns |
| --- | --- | --- |
| Crypto and memory | [crypto](mulib-secret-crypto.md), [native](mulib-secret-crypto-native.md), [OpenSSL](mulib-secret-crypto-openssl.md), [memory](mulib-secret-memory.md), [key agreement](mulib-secret-key-agreement.md) | Trusted backend configuration, approved algorithms, KDF budgets, side-channel review, and authentication of peers |
| Persistence | [vault](mulib-secret-vault.md), [record](mulib-secret-record.md), [catalogue](mulib-secret-catalog.md), [store](mulib-secret-store.md), [state](mulib-secret-state.md) | A durable transaction binding authenticated records, catalogue, counters and audit state; locking and recovery; external rollback detection |
| Authorization | [policy](mulib-secret-policy.md), [provider](mulib-secret-provider.md), [lease](mulib-secret-lease.md), [audit](mulib-secret-audit.md), [rate](mulib-secret-rate.md) | Authenticated IPC and enrolled identity, canonical requests and verified consent, atomic one-use enforcement, result validation, and persistent state |
| Local credentials | [random](mulib-secret-random.md), [passphrases](mulib-secret-passphrase.md), [OTP](mulib-secret-otp.md), [OTP URI](mulib-secret-otp-uri.md), [recovery](mulib-secret-recovery.md) | Password policy, vetted word lists, one-time recovery consumption, authorization and replay policy |
| Partial protocols | [JWS](mulib-secret-jose.md), [OAuth](mulib-secret-oauth.md), [sign](mulib-secret-sign.md), [SSH](mulib-secret-ssh.md), [AWS](mulib-secret-aws.md), [WebAuthn](mulib-secret-webauthn.md), [kind catalogue](mulib-secret-spec.md) | Complete protocol and transport, typed providers, enrollment, and interoperability |

### Build and verification

Define each implementation switch in one C or C++ translation unit, including
switches for dependencies. Consumer translation units include only declarations.
The secret APIs return `0` on success and a negative status on failure; they are
not interchangeable with upstream boolean-returning primitives. Compile the
POSIX storage implementation with `_POSIX_C_SOURCE=200809L` before system
headers. Initialize and externally synchronize caller-owned state. Do not copy
owning structs or pass overlapping buffers unless explicitly allowed.

The native backend exercises AES-128-GCM and PBKDF2-SHA256 without OpenSSL. The
optional OpenSSL adapter also provides AES-256-GCM. Argon2id and XChaCha20-Poly1305
are not available from these two adapters; do not fall back automatically.

```sh
cmake -S . -B build/secret -DMULIB_BUILD_MUW3=OFF \
  -DMULIB_BUILD_SHIRAZ=OFF -DMULIB_BUILD_VIRTIO=OFF
cmake --build build/secret --target mulib_test_secret_native mulib_secret_example
ctest --test-dir build/secret -R 'mulib_(test_secret_native|secret_example)' --output-on-failure
```

When C++ is available, build `mulib_test_secret_cpp_link` and
`mulib_test_secret_cpp_impl` to check C/C++ linkage and implementation in C++17.
Configure `-DMULIB_BUILD_SECRET_OPENSSL_TESTS=ON` explicitly, then build and run
`mulib_test_secret_openssl` for both EVP profiles and native/EVP record
interoperability, `mulib_test_secret_openssl_vectors` and
`mulib_test_secret_openssl_credentials` for primitive comparisons, and on POSIX
`mulib_test_secret_storage` for storage and memory regressions.
Use `-DMULIB_SANITIZERS=ON` for ASan/UBSan.
The optional Clang `MULIB_BUILD_FUZZER` build provides
`mulib_fuzz_secret_parsers` for bounded codec, catalogue, OTP URI, and SSH
agent message checks using synthetic inputs.

### Security boundaries

- Offline theft of the encrypted directory still permits password guessing.
  Pin the approved KDF policy before expensive work and limit concurrent unlocks.
  Compromise of the running broker or unlocked device can expose keys despite
  page locking. Lock on idle and suspend, and invalidate associated sessions.
- Treat MSV1, MSR1 and MSC1 as development wire formats. Rewrapping changes the
  password envelope, not the root; copies of older envelopes are not revoked.
  MSR1 exposes some metadata and does not bind a vault UUID. Use independent
  roots per vault. Never write a root or plaintext record to ordinary storage.
- Single-file replacement is not a transaction. An uncertain commit must be
  reconciled by an authenticated reload; never blindly retry stateful effects.
  A valid audit chain or catalogue can still be rolled back without an external
  trusted anchor.
- Raw C APIs and callback dispatch are not a trust boundary. The broker must
  resolve objects from authenticated metadata, derive identity from authenticated
  IPC, bind approval to the exact request, and never accept caller-supplied
  consent. Do not load untrusted providers into the process with the vault.
- Treat a hostile client, replayed approval, and uncertain external side effect
  as separate cases. Consume authorization before releasing a result, never
  return plaintext except to an explicitly approved destination, and reconcile
  uncertain operations instead of automatically repeating them.
- No safe On1x API is provided. Bindings should expose opaque handles only after
  the broker contract is reviewed. Do not expose these kernels or backend
  pointers through an unrestricted FFI.

Production use requires independent review, audited backend and algorithm policy,
crash and rollback testing of real persistence, verified broker authorization
and secret lifetime, and protocol-specific conformance evidence. The selftest is
not evidence that these release gates have been satisfied.
