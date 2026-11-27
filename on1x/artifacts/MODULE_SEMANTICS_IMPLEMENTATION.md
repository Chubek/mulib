# Module semantics implementation qualification (in progress)

MuLIB base: `9180386bbfce6d141bf2707aabcecf0ed856af12`.
`git diff --stat 791ea77..9180386b` shows only the normative contract.
LibStone: `4607ce58ea70482a9c057c64557260329d36afde` (no intervening revision).
Four private worktrees share a coordination record under
`/dev/shm/on1x-module-semantics-XCNFi9ny`. That record is not a build input.

| Feature | Evidence at integration |
|---|---|
| Synthesized access policy and finite binding views | Integrated `da2c8caa`; standalone executable and relocated archive tests passed in W3 worktree. Commands, dependency pin and rule digest in [MODULE_ALGEBRA.md](MODULE_ALGEBRA.md). |
| Full compiler build | Removed stdlib includes build-time compiler inputs. Standalone OML CTest does not qualify the compiler. |
| Loader graph, CLI and workspace | W1 and initial W4 commits integrated; loader/driver runtime unqualified. Graph standalone test passed in integration build. |
| Transparent type views | Provider-authenticated scalar-copy record admission tests passed; end-to-end compilation unqualified. See MODULE_TYPE_VIEWS.md. |
| Parameterized and managed type views | No expanded executable profile qualified here yet. |
| Functors, checked provider object reuse | No executable qualification yet. |
| Module-aware PPX | No negotiated transformation qualified here yet. |

The standalone rules do not establish that the compiler uses those rules for
admission. Discovery, resolution, signature checking, executable admission
and code generation need separate evidence.

## Integrated standalone qualification

Executed after integrating W1 `765e54c2`, `0c57a925`, `9dab0cd6`,
W2 `a1b9a812`, `1928c04b`, W3 `d68d3167`, and initial W4 `4ba10ae2`:

```
cmake -S on1x -B /mnt/warble/tempdir/opencode/on1x-module-semantics-XCNFi9ny-integrated-build -DON1X_MODULE_CORE_ONLY=ON -DON1X_LIBSTONE_ROOT=/mnt/nvme/libstone
cmake --build /mnt/warble/tempdir/opencode/on1x-module-semantics-XCNFi9ny-integrated-build -j2
ctest --test-dir /mnt/warble/tempdir/opencode/on1x-module-semantics-XCNFi9ny-integrated-build --output-on-failure
```

GCC 16.2.1: all three tests passed (`module_algebra`, `transparent_permit`,
`module_graph`). This explicitly excludes the compiler and stdlib-dependent
suites. No full integration gate or expanded backend profile is certified.

## Subsequent targeted external-root qualification

Worker evidence is archived in MODULE_ALGEBRA.md (nine integration tests),
MODULE_GRAPH.md (assertion-enabled loader snapshot and quotient tests),
MODULE_TYPE_VIEWS.md (strict C11 execution and sanitizer runs), and
MODULE_GRAPH_TOOLING.md (five tooling tests and project planning).
These runs used explicit external stdlib assets, not restored repository
sources. Transparent records and scalar manifest aliases printed `42`;
cross-provider, invalid-equation and unsupported-backend cases rejected
without overwriting previous outputs. Loader tests covered manifest order,
interface revisions, multiple request witnesses and overlay invalidation.

These targeted results supersede the initial per-feature pending entries
above, but do not qualify the final combined tree or general regression suite.
Generic/managed opaque transport, functors, genuine separate compilation,
and module-aware PPX transformation remain undelivered. Full project C11
publication still depends on a missing native-link housekeeping asset.
