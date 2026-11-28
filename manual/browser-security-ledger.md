# Browser security ledger

**Controlled development inputs only.** The checks below validate C11 policy
and accounting primitives, not the security of a browser or of any untrusted
document, script, plugin, device, or network workload.

| Area | Implemented and covered by offline checks | Not established |
| --- | --- | --- |
| [Origin](mulib-browser-origin.md) and [realm](mulib-browser-realm.md) | Bounded HTTP(S) tuples, strict address syntax, canonical-only equality, rejection of missing context IDs | WHATWG/IDNA, opaque and inherited origins, site and storage partitioning, CORS, realm or context isolation. The existing trailing-dot normalization is not WHATWG equivalence. |
| [Permissions](mulib-browser-permissions.md) | Empty host mask denies capabilities; unknown bits cannot add or restore authority via grant or revoke | Enforcement/interposition at protected calls, permission inheritance, user consent, or OS policy |
| [Limits](mulib-browser-limits.md) and [accounting](mulib-browser-sandbox.md) | Aliased reset from copied limits; all-or-nothing, overflow-safe response-byte/node/step/task charging; cancellation | Accounting only occurs when a caller charges. This helper does not enforce depth or URL totals, allocation or process caps, preemptive execution, or deadlines. |
| [Capability broker](mulib-capbroker.md) | Host-fixed principal and exact grant checks; frame and cumulative byte/request limits; rollback on denial or definite commit failure; indeterminate commit prevents broker reuse | Transport/authentication, per-object authority, transaction durability, audit persistence, or reconciliation after uncertain commit. A new broker must not bypass reconciliation. |

Keep permission masks, identities, accounting records, handler tables, and
transaction hooks host-owned. These helpers cannot constrain the host or a
misbehaving native callback; do not treat validated metadata as authority.
Transport and resolved peer policy are separate from this ledger. The
[Linux sandbox verification](sandbox-verification.md) records a separate
plugin fixture and does not establish browser isolation.

The offline regression test uses reserved example hosts/addresses as strings,
value records and in-memory hooks. It starts no processes, runs no scripts,
and performs no network or filesystem access:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  tests/test_browser_security.c -lm -o browser-security-test
./browser-security-test
```
