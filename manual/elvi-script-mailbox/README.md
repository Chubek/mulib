# ElviScript coordination and verification

W1, W2, W3, and W4 worked concurrently in one checkout with distinct source
ownership. Each had a bidirectional mailbox (`Wn/in`, `Wn/out`); `shared`
carried cross-worker contracts and announcements. This archive uses
`Wn/in` and `Wn/outbox` and does not contain private binaries. Paths to the
temporary spool and commands in the original messages are historical.

| Worker | Ownership | Completion report |
| --- | --- | --- |
| W1 | Elvi DSL, compiler, mode/validation, legacy regressions | [DSL](shared/005-w1-complete.md) |
| W2 | Cookie scope and HTTPS transport | [Transport](shared/022-w2-complete.md) |
| W3 | HTML/XPath, restricted Mune, fixture Lua | [Logic](shared/006-w3-complete.md) |
| W4 | Orchestration, IPC, fixtures and build wiring | [Integration](shared/049-w4-complete.md) |

See the [initial contract](shared/000-contract.md), individual mailbox
messages, [integration disposition](shared/050-coordinator-final.md) and
[post-handoff fixes](shared/051-coordinator-handoff.md).
The worker reports record the exact public interface SHA-256 digests and
their test commands.

## Verification scope

- `make check-elvi-script` runs the authorized cookie/HTTPS fixture, logic
  suite, Booking-to-receiver test, and named IPC endpoint tests. A disconnected
  receiver and injected coordinator allocation failure do not commit results.
- The Clang ASan/UBSan build passed 12 focused CTests: four legacy ElviScrape
  suites, three new script suites, the named endpoint test, two Mune runtime
  suites, and the shared allocator/hostile-input suites. The legacy provider
  tests cover all eleven pre-existing search profiles.
- Clang libFuzzer plus ASan/UBSan completed 40,000 Elvi DSL/extraction/URL
  iterations, 30,000 IPC iterations, and 30,000 cookie JSON iterations. This is
  bounded mutation coverage, not verification of every input.
- Generated MuLL inventory, KernelBind and dependency strata checks passed.
  Existing ElviScrape/Mune ExoML lint and snapshot passed; the Exotract legacy
  C11/On1x clients passed. Programmable ElviScript remains discovery-only.
- The broad `tools/check.py --no-isolation` run was not green: it passed
  1,018/1,018 module examples, 201/206 composition examples, 676/683 focused
  tests, and 11/13 combined builds. No Elvi checks were among the failures.
  Remaining failures were in HTTP/3 and X11 example invocation, graphics/
  notebook/Wayland/X11 fixtures, combined Linux networking includes, and
  three links to the absent `on1x/VERIFICATION.md`.

No live provider was contacted. The integration verifies this repository's
offline profiles, not live selectors, an independent security review, or a
production deployment.
