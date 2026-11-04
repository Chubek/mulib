# Statistical and text fixtures

`fixtures.h` contains original, deterministic inputs used by
`tests/test_statistical_text.c`: a two-state/three-symbol HMM, a tiny ASCII
corpus, and a spelling dictionary.

For observations `{0,1,2}`, exhaustive enumeration gives likelihood `0.03628`
and best state path `{0,0,1}` with joint probability `0.01512`. The corpus has
8 tokens, 5 distinct tokens, `rain=3`, `sun=2`. No external datasets, network,
random seeds, or generated binary fixtures are needed.
