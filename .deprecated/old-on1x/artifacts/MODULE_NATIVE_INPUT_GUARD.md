# Native module-input publication guard

## Fixed failure

At base `58f109f8`, `tools/on1x-native-build.py` protected its entry source and
selected tool/runtime artifacts, but not imported sources, other provider
fragments, interfaces or the project index. A successful `--emit-only` request
could replace any of those files with assembly and return zero. Native ELF
publication could likewise replace an associated `.on1xi`. Atomic replacement
alone was insufficient: the destination was an input.

Source builds now ask the real compiler for its existing
`on1x.project-module-graph/v2` physical projection. Both `sources` and `auxiliary`
inputs are protected, along with explicitly forwarded project-index,
diagnostic-catalog and PPX-schema files. The ordinary MIR emission still checks
the program; the graph is an input inventory, not executable admission.

Only ordered resolution flags are forwarded to observation. It cannot launch
a registered/source PPX or execute extensions. Without an explicit index/root,
the compiler's `--print-stdlib` response supplies the same selected root, using
the same search flags. No Python is imported from that library. The existing
IR-input and probe profiles remain unchanged. The observation shares the
native helper's overall deadline and memory/file limits; its JSON is capped
at 16 MiB and each input array at 16,384 entries. Unknown schema, malformed,
relative, missing or incomplete input inventories fail closed before assembly.

## Executed evidence

Worktree: `/mnt/warble/tempdir/opencode/module-hardening-native-guard`.
Test artifacts: `/mnt/warble/tempdir/opencode/module-hardening-native-guard-build`.
No deleted assets were restored.

```
python3 -B on1x/tests/test_native_input_guard.py \
  /mnt/warble/tempdir/opencode/module-hardening-debug-build/on1xc \
  /mnt/warble/tempdir/opencode/module-hardening-native-guard-build -v
```

**11 tests passed in 2.476 seconds.** This used the real combined Debug frontend
from worker revision `d7c71b841de6a8f5a8e88b24bcfe37480b055775`;
compiler SHA-256:
`63436494dc6f26be61ec3bee0f579185f197367c5448b1e34c770c0204c4e44c`.
Its explicitly selected external library was
`/mnt/nvme/fpnext.mI6yYw/mulib/on1x/stdlib`, checkout `cbb7d66f`, tree
`bb7e7814635bb6e161e4c092b2129c493ec89a88`.
Python was 3.14.7; `/usr/bin/gcc` 16.2.1; GNU assembler/linker/ar 2.47.

Coverage includes a two-fragment provider, associated abstract interface,
legacy import, default-library discovery, paths with spaces/Unicode, entry and
every provider/signature/index destination, symlink aliases, preservation of
successful output after body-check rejection, and zero PPX execution during a
rejected observation. Positive assembly replacement and real ELF
assemble/link/publication/execution succeed; the ELF returns 42. The IR-input
compatibility case succeeds. Protocol negatives include unknown versions,
missing entry, malformed arrays/paths, missing files and excessive counts.
Search-flag order and exclusion of transformer/checker flags are checked.

The backend fixture **substitutes deterministic AMD64 assembly generation**.
MIR production/body checking use the real frontend; ELF assembly/linking use
real tools. This qualifies helper publication behavior, **not actual Archon
MIR lowering or a new module ABI**.

Against the extracted `58f109f8` helper, the two destination/alias tests fail
in seven independent subcases because the old helper returns success after
replacing the input or alias. The separate executable/signature negative also
fails against the old helper, which successfully publishes an ELF over the
interface. These are deliberate baseline failures, not skipped regressions.
Set `ON1X_TEST_NATIVE_HELPER` to the extracted helper to reproduce them.

## Limits and handoff

This adds one bounded nonexecuting parse/resolution pass to source builds.
It does not create a cross-process immutable source snapshot or solve hostile
concurrent filesystem mutation between observation and compilation. It does
not discover native PPX program binaries or every external tooling input.
Actual Archon lowering, managed/generic module ABIs, functors and separate
compilation remain unqualified. Full combined CTest registration/rerun is a
parent integration handoff; no shared CMake, CLI, loader, signature or central
report file was edited by this worker.
