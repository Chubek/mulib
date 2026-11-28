# Host toolchain probe verification

2026-11-23, Linux x86-64 with GCC 16.2.1, Clang 22.1.8, GNU binutils
2.47, and Python 3.14.7. Base MuLIB revision
`87cbf7c361667c62f82759bd0c02834b2b07315c`, pinned LibStone
`4c35077601899944594737559534aafd9014f330`; LibStone was unchanged.
The default temporary filesystem was full; usable executable space was
provided with `TMPDIR=/tmp` for the commands below.

```sh
TMPDIR=/tmp python3 -B -m unittest discover -s on1x/toolchains/tests -v
TMPDIR=/tmp ctest --test-dir build/kernel-shared-general --output-on-failure \
  -R '^on1x\.(compiler|cephyr\.native|memgraph\.c11)$'
TMPDIR=/tmp ctest --test-dir build/kernel-shared-general --output-on-failure \
  -R '^on1x\.(cephyr\.(native|clamp_abi|options|optimization)|exotract)$'
TMPDIR=/tmp ctest --test-dir build/firestone-hir --output-on-failure \
  -R '^on1x\.(compiler|firestone_hir)$'
TMPDIR=/tmp ctest --test-dir build/sync-cephyr --output-on-failure \
  -R '^cephyr\.(c-api|cpp-api|python-api|c11-sample)$'
```

- Probe suite: 11 passed, no skips. Both GCC and Clang were tested against
  the real assembler and linker. Two GCC runs produced byte-identical JSON.
- `kernel-shared-general` (Cephyr, Exotract, MEMGraph enabled): 3/3 baseline
  and 5/5 focused CTests passed, with `cephyr.native` common to both runs.
- `firestone-hir` (Firestone enabled, Cephyr and Exotract disabled): 2/2 passed.
- Standalone cached Cephyr: 3/4 passed. `cephyr.c-api` still fails at
  `test_native_artifacts`, the pre-existing failure in the
  [main verification record](../VERIFICATION.md).

Only the native glibc non-PIE profile is covered. These are focused checks,
not a whole-suite result, a transitive toolchain lock, or foreign ABI evidence.
