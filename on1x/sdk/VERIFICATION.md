# SDK 2 verification record

## Baseline

On 2026-11-19, before SDK 2 changes:

```sh
cmake --build build/on1x-sdk-gcc-make \
  --target on1x_sdk on1x_sdk_test_registry on1x_sdk_test_analyzer \
  on1x_sdk_test_backend on1x_sdk_test_security on1x_sdk_test_c_abi \
  --parallel 4
ctest --test-dir build/on1x-sdk-gcc-make --output-on-failure \
  -R '^on1x\.sdk\.'
```

Result: 8/8 tests passed, including installed consumer, isolated dynamic plugin,
linked compiler provider, registry, analyzer, backend, security, and strict C
ABI tests. This establishes only the existing ABI-v1 profile.

## SDK 2 acceptance

Each public facility needs positive, negative, limit, lifecycle and ownership
tests. Cross-process facilities additionally need malformed protocol, stale
handle, capability-denial, cancellation and worker-failure tests. Candidate IR
facilities need rejection by authoritative verification and stale-revision
tests. Build/package facilities need installed and relocated consumers.

The final matrix must record exact GCC/Clang, generated/fallback scanner,
debug/release, sanitizer, ABI-v1/v2, plugin, embedding, runtime, backend and
allocation-failure results. A declaration or registered name is not execution
evidence.
