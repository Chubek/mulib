# On1x completion ledger

**2026-11-26** · qualification base:
`26c4fc0845be2ffd120ce0d68b4b7c5ee7c95be9` (MuLIB),
`76d24a7f8b19294bab2b713dc69f799e412b62d2` (bundled LibStone).

| Question | Current record |
| --- | --- |
| What source/product/target profile is implemented or missing? | [Capability matrix](docs/capability-matrix.md) |
| What actually passed, failed, skipped or was not run on this task? | [Release qualification and evidence](docs/release-qualification.md) |
| What did earlier profiles demonstrate? | [Verification index](VERIFICATION.md) and the [historical consolidation increment](docs/consolidation-status.md) |
| What is intentionally left open? | [Roadmap](docs/roadmap.md) and the exclusions in each capability row |
| What can this SDK session/provider admit? | [SDK capability matrix](sdk/CAPABILITY_MATRIX.json) and [backend routes](docs/backend-phase-audit.md); this ledger does not grant capability |

### Scoped qualification completed on 2026-11-26

| Scope | Status |
| --- | --- |
| [Compiled scanner selections and On1xMETA processor artifacts](docs/release-qualification.md#on1xmeta-provenance-and-snapshots) | **VERIFIED** for the focused generated and forced-fallback configurations, including the Perl-free processor probe. |
| [Linked backend report](docs/release-qualification.md#linked-backend-capability-reporting) | **VERIFIED** for the report fixture, not for request authorization or external toolchain readiness. |
| [Explicit nominal-owner MIR](docs/release-qualification.md#nominal-owner-mir) | **VERIFIED** for the named in-memory verifier, evaluator and C11 fixture and tested legacy ingress rejection. No SDK or native owner execution. |
| [Static PriceOpt fixture](docs/release-qualification.md#native-kernelbind-fixture) | **VERIFIED** in the named manual runs and the generated optional CTest with a documented historical parser bootstrap. No production approval. |
| [Rebuilt Cephyr FFISpec](docs/release-qualification.md#results-and-limits) | **VERIFIED** for the generator fixtures and isolated full scans with an explicit historical QLD bootstrap; not an On1x combined run with those libraries. |
| [Restricted stdlib staging](docs/release-qualification.md#restricted-stdlib-staging) and [installed consumers](docs/release-qualification.md#results-and-limits) | **VERIFIED** for the named fallback SDK/project consumer and generated moved Archon project checks, not for all optional runtimes. |

These rows close only the named tests, not the release. See the
[release checklist](docs/release-qualification.md#release-checklist) for
the first combined run's limits and the principal's final accounting.

This index replaces the missing current documentation entry point. The removed
[2026-11-25 completion snapshot](https://github.com/Chubek/mulib/blob/faa82816461b961614b16cf81df8a5d40e2ff155/on1x/COMPLETION_MATRIX.md)
and its [archived JSON](https://github.com/Chubek/mulib/blob/faa82816461b961614b16cf81df8a5d40e2ff155/on1x/COMPLETION_MATRIX.json)
describe that revision only. No new parallel machine-readable inventory is
maintained here.
