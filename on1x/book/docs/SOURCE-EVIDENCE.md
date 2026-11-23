# Source evidence used for editorial examples

## Reference manuscript

The book links implementation sources at `2cdceb9c`. For the ownership
lab, the compiler was built at `6b718205` (the intervening commit changed
only the book), with the fallback scanner and direct C11 on x86-64 Linux,
GCC 16.2.1. The list example printed `13`; the lifecycle fixture and its
early-drop variant produced the counts in Appendix A. `--check` rejected
the double-propagation fixture at its second `pending?`. The
`on1x.recursive_owning_adts` CTest passed in this profile.

## Original typesetting toolkit

The toolkit's syntax highlighting is not a claim that all grammar constructs execute.

- On1x README, lines 1--24, revision `3eaebde74d1093cd07242afa904fa6dd50781af7`:
  `on1x/README.md`, SHA256 `ffd043ed54230978b33f1b1380e47dcc4b2e0df1885e7f39ebc32d64be45f7f2`.
  Describes a statically compiled language, the C11 backend, HIR and MIR.
- On1x README, lines 97--120, same revision/hash:
  cooperative fiber/channel profile is described as executable; async/await is
  explicitly **not implemented**. Do not generalize this beyond that snapshot.
- `on1x/ON1X_CORE_SYNTAX_ADDITIONS.md`, lines 383--406:
  SHA256 `83eec5547ea5a45a0eebb7353186cfac121d35c25cfbac2f9b44b0d30bff27ac`.
  Presents design examples, not proof of executable support.
- `on1x/docs/capability-matrix.md`, lines 74--81:
  SHA256 `45e34b14c9fa968c4f898999145e283dd43579271c287fec5bff7e2eb6746a55`.
  Capability status is scoped by backend, profile, and tests.

The original toolkit smoke tests check LaTeX typesetting functionality.
