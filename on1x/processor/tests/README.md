# On1xMETA verification

See [the dated execution record](VERIFICATION.md) for commands, results and
known baseline failures.

The tests use the production frontend and processor. Generated-rule fixtures
are compiled with the real LLSynth parser; they do not implement a second
On1xMETA parser or replacement compiler.

| Test | Boundary exercised |
| --- | --- |
| `on1x.processor.generator` | Fresh LLSynth generation, deterministic parser/tables/manifests, input relocation and ordering, C++ schema compilation, malformed syntax, guards, capture multiplicity/effect order, decreasing rewrite measure, overlap, budgets, encoding, output preservation |
| `on1x.processor.snapshots` | Checked-in parser/table snapshots and pinned generator provenance |
| `on1x.processor.rule_add`, `rule_subtract` | Same source and production frontend, different authored `.o1m` match: checked return becomes `Variable` or remains `Binary`; both evaluate to 42 |
| `on1x.processor.rule_forbid` | Authored semantic constraint changes source acceptance, with source and rule provenance |
| `on1x.processor_engine` | Structural matching, metadata/hygiene, fresh-name collision avoidance, fixed points, resource exhaustion and transactional rollback |
| `on1x.processor_semantic` | Malformed HIR, checked annotations and semantic constraint enforcement |
| `on1x.processor_integration` | Actual rule hits and provenance, all semantic phases active, original invalid source rejection, source execution across supported backends |

The integration test compares independently specified output traces, exit
statuses and terminal traps across strict HIR-C11, MIR-C11, optimized MIR-C11,
MIR evaluation and real Archon native execution. Closures and unique owners
use the C11 profile with allocation counters. The scalar tests cover evaluation
order, short-circuiting, integer limits, floating-point signed zero/NaNs, loops
and a seeded boolean oracle. Native execution is explicitly reported
unsupported if the configured compiler build has no Archon tool and scalar
ABI archive.

The engine test checks all eight literal `&&`/`||` combinations against
their truth tables: each becomes a literal in one compiled desugaring step.
Only the closed `false && false` and `true || true` rules have no capture.
For `false && mark()` and `true || mark()` the test finds the original call
in the unselected branch; for `mark() && false` and `mark() || true` it
finds the call in the evaluated condition. The generator tests independently
reject rules that drop either evaluated or skipped captures.

```sh
ctest --test-dir BUILD --output-on-failure -R 'on1x\.processor'
python3 on1x/processor/tests/test_generator.py \
  --root on1x --work WORK/generator --cxx c++
python3 on1x/tests/test_processor.py \
  BUILD/on1xc "$PWD/on1x" WORK/processor cc
python3 on1x/tests/test_processor.py \
  BUILD/on1xc "$PWD/on1x" WORK/processor-sanitized clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer
```

The sanitizer command instruments generated C11, the C runtime and allocation
fixtures. It does not instrument the prebuilt compiler, scalar ABI archive or
native assembly. Engine-only sanitizer coverage is described in
[`../REWRITES.md`](../REWRITES.md). Full source, independent-checker, scanner and
SDK regression tests remain separate compatibility gates.
