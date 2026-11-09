# mulib-chem

[Manuals](README.md) · [Library index](index.md)

Formula notation/composition, approximate molar masses and exact bounded
stoichiometric balancing. This is not a molecular simulator, reaction predictor,
SMILES/IUPAC-name reader, equilibrium solver or laboratory/safety reference.

Define CHEM, SYMBOL, MATHML, TEX, FRACTION, XML, STRBUF and UTF8 switches in one
translation unit. Link math where required. The [example](../examples/modules/chem.c)
uses the shared explicit [notation recipe](../examples/notation-common.h).

## Formula profile

`parse` accepts <=64 KiB XML-valid UTF-8 with no embedded NUL. Empty input fails.
Supported syntax:

- All 118 element symbols, positive integer counts: `H2O`, `NaCl`.
- Nested parentheses/brackets: `Ca(OH)2`, `[Fe(CN)6]^3-`.
- Isotope mass numbers: `[13C]O2`, `[2H]2O`.
- Initial/component coefficients and hydrates: `CuSO4·5H2O` or `CuSO4.5H2O`.
- A final explicit `^+`, `^-`, `^2+`, etc. charge, magnitude <=100.
- Optional trailing phase `(s)`, `(l)`, `(g)`, `(aq)` after charge/formula.
- `e^-` as an electron (charge -1, no atom count, zero reported mass).

No whitespace, fractional stoichiometry, zero counts, bare trailing charge signs,
unknown symbols or unbalanced/empty groups are accepted. Isotope mass numbers
must be between atomic number and 400; this is syntax screening, **not** proof
that an isotope exists. Grouping depth <=32, <=4096 notation nodes, and total
atoms <=10^12. Count multiplication/addition is checked before overflow.

`parse` returns an owned formula or NULL with a static source-byte diagnostic.
Source is copied into structured XML nodes/metadata and cached composition;
borrowed source can be released after return. `destroy` is NULL-safe. The tree
accessor returns a read-only borrowed tree: do not mutate it, since the cached
composition/mass would no longer agree. Phase/text/element descriptors are
borrowed. `count` uses atomic number, including isotopes in their element total.

## Masses and notation

The element table provides illustrative conventional atomic-weight approximations
in g/mol. Values for unstable elements are selected integer mass numbers. It does
not carry isotope abundances, uncertainties, interval atomic weights, revision
certification or substance-specific properties. Isotopically labelled atoms use
their **integer mass number** for approximate mass, not exact isotopic mass.
Charge does not change reported mass; electron mass is ignored. Do not use these
numbers for safety decisions or high-precision analytical chemistry.

MathML returns a new owned root; LaTeX returns malloc-owned math-mode notation
without delimiters. Counts, isotope superscripts, explicit groups, hydrate dots,
charges and phases are preserved. Element symbols are upright in both outputs.
Optional lengths exclude NUL and are zero on failure; no renderer or external
TeX program is invoked. The borrowed tree retains component/group structure and
charge/phase metadata. Queries return zero/NULL on invalid arguments as documented
by their public declarations.

## Exact balancing

Supply reactants then products to `balance`, with 1..8 species on each side and
<=16 total. It forms element and charge conservation equations, uses checked
normalized signed-64-bit rationals to reduce them, and requires a one-dimensional
nullspace with a strictly positive solution. Success returns 1 and reduced positive
integer coefficients. Failure returns 0, leaves output unchanged and reports a
static diagnostic. Formula coefficients already present are part of the supplied
species, not stripped before balancing.

Ambiguous nullspaces, impossible reactions, zero/negative coefficients, isotope-
specific formulas and conservative arithmetic-overflow cases are rejected.
Isotope-specific conservation is not silently approximated. Charge balancing
supports explicit electrons, but does not introduce water, protons, electrons or
other missing species automatically. Balance is conservation only, not evidence
that the reaction occurs or is chemically sensible. There is no evaluation of
conditions, kinetics, mechanism, thermodynamics or hazards.

Independent objects have no shared mutable state. Work is bounded by formula
limits and a 119-by-16 rational matrix. Tests include hydrates, groups, ions,
isotopes, mass approximations, redox charge conservation and failure cleanup in
[test_notation.c](../tests/test_notation.c) and
[test_notation_alloc.c](../tests/test_notation_alloc.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-chem.h`](../include/mulib-chem.h) |
| Implementation switch | `MULIB_IMPL_CHEM` |
| Repository-local includes | [mulib-fraction](mulib-fraction.md), [mulib-symbol](mulib-symbol.md), [mulib-tex](mulib-tex.md) |
| Complete example | [`examples/modules/chem.c`](../examples/modules/chem.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/chem.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
