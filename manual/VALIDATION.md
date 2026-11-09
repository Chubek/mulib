# Networking validation report

The 92 networking modules are registered in `MANIFEST.yaml`; the total inventory
is 757 implemented modules, plus four auxiliary headers. Each networking module
has a manual, a standalone checked example, and a symbol/link test. These tests
are supplemented by six cross-module behavioral/regression suites.

Verified on Linux:

- GCC strict C11 CMake build and **401/401 CTest cases** passed.
- **98/98 networking test sources** built and ran with both GCC and Clang, `-Wall -Wextra
  -Wpedantic -Werror`, ASan and UBSan (nonrecovering UB checks).
- **92/92 networking module examples** built and ran with strict GCC warnings.
- All 12 top-level networking demonstrations compiled with strict GCC warnings.
- The same 12 demonstrations compiled with strict Clang warnings.
- C++17 consumers linked and ran against separately compiled C11 implementations.
- All **2271 isolated-header cases** passed with both GCC and Clang. The two
  Netfilter headers were rechecked in isolation after correcting their
  kernel/libc include-order conflict; reverse-order and late-implementation
  composition also passed syntax checks with both compilers.
- Manual links/source references, dependency strata, and the regenerated MuLL
  inventory passed their consistency checks (761 headers).
- MuLL C++ and D hosts, optimized/unoptimized cross-host conformance (including
  a new networking fixture), and bridge inventory/regression checks passed:
  **6/6 selected MuDSL CTest cases**.
- Supplemental library-wide `test_hardening.c` passed Clang ASan/UBSan at `-O1`;
  `test_hostile_inputs.c` passed at `-O0`. The latter triggered a Clang 22.1.8
  optimizer crash at `-O1` with sanitizers, so that optimized configuration is
  not claimed as verified.
- Supplemental optimized GCC builds of both library-wide hardening suites
  passed when run with a longer compiler timeout.

The optimized checker encountered intermittent GCC/Clang compiler crashes and
90-second compiler timeouts on some runs. Clang's failed `prowse-protocol` and
`test_storage_server_alloc` cases passed individual optimized retries. These
toolchain failures are not being suppressed or treated as successful checker
invocations; the complete CTest and networking sanitizer results above are
separate successful runs.

After the include-order correction, both optimized checker runs passed all
**10/10 combined builds**. GCC also passed **760/760 module examples** and
**15/15 composition examples**. Its only remaining failure was an internal
compiler error in the existing `test_hardening.c` build (`mulib-mune.h`), which
passed the separate optimized retry noted above. Clang's final run failed only
the two existing compiler-crash cases noted above; both passed optimized retries.
Neither full checker invocation is reported as an uninterrupted clean run.

The regressions cover unaligned wire buffers, extended ACK echoes and truncated
attributes, dump interruption/overrun, sequence wraparound, checksum carry
overflow, ARP replacement/expiry, fragment overlap and final-length conflicts,
DNS compression errors, RSN truncation, and D-Bus authentication/framing bounds.
Read-only Netlink examples queried real kernel state. Optional service and
hardware queries report their availability; this does not certify compatibility
with NetworkManager, systemd, iwd, or wireless hardware.

Privileged mutation/namespace integration, sustained adversarial traffic,
coverage-guided fuzzing, and full RFC/daemon interoperability remain outside
this verification. Tests did not modify host routes, interfaces, DNS, Wi-Fi or
firewall rules. The focused APIs are not a complete TCP/IP stack or network
manager.
