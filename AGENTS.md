# The Current Task: Rewriting MuLL and the MuLL Book

MuLL (written at `mudsl/mull`) must be rewritten. The book for MuLL, at `mudsl/book` must be rewritten as well.

## MuLL Implementation

MuLL is written with the aide of `mudsl/MuDSL.hpp`. This won't change. All facilities of MuDSL are still in use. What we want to add is to use `MuDSL.hpp`'s rewrite tools (specifically the equality saturation facilities) to implement an intermediate layer for MuLL which will be used analysis, and transformation. The analysis and transformation will be used for semantic analysis, and optimization.

The intermediate format is called MuLLIER, implemented in `mudsl/mull/mullier`. It relies on S-Expressions (using MuDSL's s-Expression parser) for analysis and transfromation. These S-Expression files are stored in `mudsl/mull/rewrite`. Analysis files have the extension `.a` and transformation files have extension `.t`. The MuLLIER uses these files in tandem to achieve optimization and semantic analysis.

Furthermore, the standard library for MuLL must be expanded. I want 30 new standard libraries.

Also, change the directory structure to make more sense. At the moment, everything is in `stdlib`. It does not make sesne. Rearrange everything under `mudsl/mull`.

## MuLL Book

1. The MuLL book at `mudsl/book` must be expanded to include `MuDSL.hpp` and `MuDSL.d`.
2. The MuLL book must expand its delve into MuLL. It should discuss both MuDSL and MuLL more in depth.
3. The MuLL book must use:
  - `typeset-manuals.py` to compile `manual` directory (at root) to LaTeX and include them in the book. This script relies on `mudsl/book/md2latex.py`, which is already implemented and provided.
  - `typeset-manifest.py` to compile `MANIFEST.yaml` and include them into the book.
4. Improve  the use of LaTeX. Improve the typesetting. For example, use `minted` for code listings, and create a `minted` syntax for MuLL.
