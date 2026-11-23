# Upstream references used to design MEMGraph

Read via the connected GitHub repository API on 2026-11-21.

- LibStone: `Chubek/libstone` at `b95c74aa61a28e85251c6173d0de43c09ca08b32`.
  See `cornerstone/LLSynth/README.md`, `cornerstone/LLSynth/examples/typed/double.llsynth`,
  `cornerstone/LLSynth/docs/types.md`, `cornerstone/LLSynth/tests/cli.py`,
  and `cornerstone/LLSynth/scripts/llsynth.pl`.
- MuLIB/On1x: `Chubek/mulib` at `fea826281236555e6e4996438c4a7487e95c2723`.
  See `on1x/processor/On1xMETA.llsynth`, `on1x/processor/README.md`,
  `on1x/processor/INTERFACE.md`, `on1x/include/on1x/ast.hpp`,
  `on1x/src/canonical/verify_values.cpp`, `on1x/docs/core-references.md`,
  `on1x/include/on1x/safety.hpp`, `on1x/docs/contracts.md`,
  `on1x/docs/recursive-owning-adts.md`.

The initial design archive did not run LLSynth. The integrated build now runs
the configured LibStone generator; no generator sources are copied. See
[README.md](README.md) for regeneration and [verification](../../VERIFICATION.md)
for the tested On1x and LibStone revisions.
