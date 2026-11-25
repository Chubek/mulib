FROM: W2
TO: coordinator, W4
SUBJECT: Authenticated HTTPS and cookie store complete
STATUS: implemented and tested offline
DEPENDS: W1 URL policy and verified TLS connector; common detail types; HTTPS/TSL/HTTP
INTERFACE: kernels/mulib-elvi-script-transport.h (MULIB_IMPL_ELVI_SCRIPT_TRANSPORT)
BODY:
Owned changed files:
- `kernels/mulib-elvi-script-transport.h` SHA-256 `ea4be9df3a00a2905512e24e6873a3110a6e9658632ef59f600a1e6c554228ba`.
- `tests/test-elvi-script-transport.c` SHA-256 `3e06daf2a21ef5a9a4f0c912a83f3d0afaf97667e16af1997fd036411d590a3e`.
- `manual/elvi-script-transport.md` SHA-256 `61902535d7f1e7640adcfb3e5f834ad9602408686f62b7d4e22be09d482e4c20`.
The final API is unchanged from `020-w2-interface.md`; implemented using W1's exported shared verified TLS connector and common types under `detail/`. W4's live wrapper already uses the matching cookie-load API. No legacy file edits.
Tests (each prints `elvi-script transport: OK`): `TMPDIR=/tmp cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Ikernels tests/test-elvi-script-transport.c -lm -o /tmp/mulib-elvi.HUPz4MaF/W2/test-elvi-script-transport` and run; CLANG with the same strict flags and run; GCC with `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie` and run with `ASAN_OPTIONS=detect_leaks=1`. Standalone transport implementation also compiles without POSIX feature flags (loader returns NULL on unsupported builds). Fixture tests cover secret header matching and suppression, frozen-clock expiry boundary, host binding, TLS before write, rejected redirects with one connection, typed binary response, body/raw caps, file policy, malformed schema/framing, and every allocation fail point through cookie parse/load and authenticated HTTP. No network calls, provider testing, or bypass.
Limits: exact HTTPS host/default port and fixed first-party SameSite context, no redirect/retry/Set-Cookie mutation/compression/artifact retention. A trusted TLS backend must vet DNS/destination addresses and enforce finite I/O deadlines. JSON cap 64 KiB, 64 cookies, 8192-byte Cookie value; body 2 MiB, wire cap body+64 KiB. File load POSIX feature flag required; authorized in-memory parsing remains available. Parent domain imports cannot broaden the exact profile host, and there is no PSL implementation. No further changes planned.
