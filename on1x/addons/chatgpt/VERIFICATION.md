# Addon verification

Scope: M7 read-only knowledge retrieval and a narrow tool-only MCP
Streamable HTTP adapter. **PARTIAL** integration: local behavior is testable;
hosted TLS/mTLS/OAuth deployment and actual ChatGPT web tool selection require
operator validation. This does not upgrade any On1x language/kernel status.

Measured 2026-11-19, continuing the existing assignment on **GPT-6.1 Sol**.
MuLIB HEAD `b66da8f5150adca2a3416db747d91bf81a3fe138` plus shared
uncommitted worktree; in-checkout LibStone
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d`. Linux
6.18.52-1-lts x86_64; Python 3.14.7; GCC/G++ 16.2.1 (`20260810`);
Clang/Clang++ 22.1.8. No compiler, PPX or kernel is invoked by the addon.

| Check | Measured result |
|---|---|
| Direct Python suite, including real repository smoke | **27/27 passed** |
| Python development mode (`-X dev`) suite | **27/27 passed**, no resource warnings |
| New CTest registration `on1x.addon_chatgpt` | **1/1 passed** |
| CDD regression suite, GCC and Clang | **PASS** on each |
| CDD regression suite, ASan/UBSan, GCC and Clang | **PASS** on each |
| Existing configured root integration suite | **683/683 passed**; existing binaries, no full rebuild |
| Scoped `git diff --check` | **PASS** |

Real data smoke: **1,188 canonical modules**, **2,522 indexed files**,
**14,961,444 bytes**, **16,111 chunks**. This is measured evidence, not a fixed
inventory or expected future count. Allowlisted snapshot SHA256:
`49ee615dbf9180f30a591ef683486f2793461b30ab09f08141b81b4f6f51f69e`.
Allowlisted LibStone README/STATUS fingerprint:
`76879e86f936ea7cd72ab6451366552c46e7e6e335765dda1d572c799fd5b7fc`.
Changes to shared source documentation legitimately change those fingerprints.

```sh
python3 -B on1x/addons/chatgpt/test_chatgpt.py
python3 -B -X dev on1x/addons/chatgpt/test_chatgpt.py
python3 -B on1x/addons/chatgpt/server.py --root "$PWD" --check

cmake -S on1x -B build/chatgpt-addon-tests -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_WITH_SYREN=OFF -DON1X_WITH_WETSTONE=OFF
ctest --test-dir build/chatgpt-addon-tests --output-on-failure \
  -R '^on1x\.addon_chatgpt$'

TMPDIR=/mnt/nvme/mulib/build/chatgpt-validation-tmp \
  ctest --test-dir build/cdd-root-gcc --output-on-failure --parallel 4
```

CDD commands, from the repository root, with an existing NVMe temporary directory:

```sh
TMPDIR=/mnt/nvme/mulib/build/chatgpt-validation-tmp \
  CC=gcc CXX=g++ sh tests/run-cdd.sh
TMPDIR=/mnt/nvme/mulib/build/chatgpt-validation-tmp \
  CC=clang CXX=clang++ sh tests/run-cdd.sh
TMPDIR=/mnt/nvme/mulib/build/chatgpt-validation-tmp \
  CC=gcc CXX=g++ \
  CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' sh tests/run-cdd.sh
TMPDIR=/mnt/nvme/mulib/build/chatgpt-validation-tmp \
  CC=clang CXX=clang++ \
  CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' sh tests/run-cdd.sh
```

Initial runs using `/tmp/opencode` encountered a full `/tmp` tmpfs:
the Clang sanitizer example link failed and root
`mulib_mune_runtime_backends` failed allocating temporary data. No unrelated
files were deleted to reclaim space. Moving temporary outputs to the isolated
NVMe directory allowed the full Clang CDD sanitizer suite and complete root
683-test rerun to pass. GCC/Clang unsanitized and GCC sanitizer CDD runs also
passed before that change.

The tests cover deterministic indexing and relocation; exact source hashes/
line citations; immutable request-time snapshots; kernel ambiguity/missing
names; separate ABI/contract/implementation/admission states; source pin drift;
malformed manifest fields/counts/duplicates/dependencies; symlinked parents and
final files; FIFOs; unallowlisted secret files; byte/file/chunk/query/work/output
limits; hostile literal query text; JSON duplicates/depth/encoding; tool schemas/
annotations; MCP initialization/list/call/errors/notifications; HTTP Host/Origin,
body/content/Accept/protocol checks, missing routes, disabled SSE; deterministic
packaging and execution after relocation; real repository indexing and queries.

Unsupported / not exercised by these offline tests: actual ChatGPT credentials
or web account; MCP Inspector interoperability; hosted TLS/mTLS, OAuth gateway,
public-directory review; MCP 2.0 discovery, SSE streams, events/tasks; full
On1x compiler suites and full repository sanitizer suite. Python-only addon changes
introduce no C/C++ ABI or compiler semantic changes. Passing source retrieval
tests is not On1x feature verification.
