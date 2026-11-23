# On1x Book Module and API Reference

The toolkit has **107 feature `.sty` modules**, one aggregator `on1x-suite.sty`, and one document class `on1x-book.cls`.

Use `\documentclass[full,openany]{on1x-book}` to load all features.
For individual modules in an existing `book` document use `\usepackage{on1x-grammar}` etc.

Named environments are invoked with `\begin{...}` / `\end{...}`.
Not every public macro has an identical argument signature; examine its `.sty` file for exact usage.

## Code

### `on1x-api.sty`
Compact declaration/description documentation.

**Commands:** `\OnxAPIEntry`
**Environments:** `OnxAPITable`

### `on1x-code.sty`
On1x code listing language and baseline style.

**Commands:** `\OnxInputCode`
**Environments:** `OnxCodeBlock`

### `on1x-code-diff.sty`
Patch hunks and before/after code comparisons.

**Commands:** `\OnxBeforeAfter`
**Environments:** `OnxDiff`

### `on1x-code-files.sty`
External code snippet inclusion from source files.

**Commands:** `\OnxCodeFile`, `\OnxCodeRange`
**Environments:** —

### `on1x-code-inline.sty`
Safe inline code for short literal strings.

**Commands:** `\OnxCodeText`, `\OnxInline`
**Environments:** —

### `on1x-code-lines.sty`
Numbered code blocks for discussion by line.

**Commands:** `\OnxCodeLine`
**Environments:** `OnxNumberedCode`

### `on1x-commands.sty`
Command-line option and executable documentation.

**Commands:** `\OnxOption`
**Environments:** `OnxOptions`

### `on1x-diagnostics.sty`
Formatted compiler diagnostics.

**Commands:** `\OnxErrorCode`, `\OnxSourcePosition`
**Environments:** `OnxDiagnostic`

### `on1x-output.sty`
Compiler output and stdout samples.

**Commands:** —
**Environments:** `OnxOutput`

### `on1x-repl.sty`
Interactive On1x input/output session panels.

**Commands:** `\OnxREPLPrompt`
**Environments:** `OnxREPL`

### `on1x-terminal.sty`
Shell commands and terminal output blocks.

**Commands:** `\OnxPrompt`
**Environments:** `OnxShell`

### `on1x-testing.sty`
Test cases with expected outcomes.

**Commands:** `\OnxExpect`, `\OnxObserved`
**Environments:** `OnxTestCase`

## Editorial

### `on1x-compatibility.sty`
Compiler version and profile requirement notes.

**Commands:** `\OnxProfile`, `\OnxRequiresVersion`
**Environments:** `OnxCompatibility`

### `on1x-editorial.sty`
Internal manuscript revision marker.

**Commands:** —
**Environments:** `OnxEditorial`

### `on1x-feature-matrix.sty`
Capability support tables for documented versions.

**Commands:** `\OnxFeatureRow`
**Environments:** `OnxFeatureMatrix`

### `on1x-footnotes.sty`
Footnote rules and note symbols.

**Commands:** `\OnxSideFootnote`
**Environments:** —

### `on1x-marginalia.sty`
Short marginal commentary.

**Commands:** `\OnxMargin`
**Environments:** —

### `on1x-provenance.sty`
Source revision, file/line and hash citations.

**Commands:** `\OnxRevision`, `\OnxSourceHash`, `\OnxSourceSpan`
**Environments:** `OnxEvidence`

### `on1x-quotations.sty`
Technical quotations and attributed excerpts.

**Commands:** `\OnxAttribution`
**Environments:** `OnxQuote`

### `on1x-roadmap.sty`
Roadmap staging and explicit milestone status.

**Commands:** `\OnxRoadmapItem`
**Environments:** `OnxRoadmap`

### `on1x-sidebar.sty`
Long-form side commentary kept within the text block.

**Commands:** —
**Environments:** `OnxSidebar`

### `on1x-statuses.sty`
Feature maturity statuses, independent of kernel admission.

**Commands:** `\OnxAbsent`, `\OnxExecutable`, `\OnxFeatureStatus`, `\OnxPartial`, `\OnxStatusBadge`, `\OnxVerified`
**Environments:** —

### `on1x-timelines.sty`
Milestone lists for compiler evolution.

**Commands:** `\OnxMilestone`
**Environments:** `OnxTimeline`

## Formal

### `on1x-contracts.sty`
Preconditions, postconditions and invariants.

**Commands:** `\OnxEnsures`, `\OnxHoare`, `\OnxInvariant`, `\OnxRequires`
**Environments:** —

### `on1x-definitions.sty`
Formal terminology and propositions.

**Commands:** —
**Environments:** `OnxAxiom`, `OnxDefinition`, `OnxProposition`

### `on1x-effects.sty`
Effect annotations and resource judgment notation.

**Commands:** `\OnxCapability`, `\OnxEffectJudgment`, `\OnxEffectRow`, `\OnxEffectType`
**Environments:** —

### `on1x-equations.sty`
Numbered aligned equations with semantic commentary.

**Commands:** `\OnxEqReason`
**Environments:** `OnxDerivation`

### `on1x-inference.sty`
Prooftree-style inference rules.

**Commands:** `\OnxRule`
**Environments:** `OnxRules`

### `on1x-judgments.sty`
Formal typing and evaluation judgments.

**Commands:** `\OnxEntails`, `\OnxEvaluation`, `\OnxStep`, `\OnxStepStar`, `\OnxTyping`
**Environments:** —

### `on1x-math.sty`
General math symbols, vectors and set operators.

**Commands:** `\OnxDom`, `\OnxFV`, `\OnxMapsTo`, `\OnxPartialMap`, `\OnxSet`, `\OnxTuple`
**Environments:** —

### `on1x-matrices.sty`
Matrix and grammar analysis notation.

**Commands:** `\OnxCases`, `\OnxMatrix`, `\OnxVector`
**Environments:** —

### `on1x-notation.sty`
A legend for conventions used in the book.

**Commands:** `\OnxNotationRow`
**Environments:** `OnxNotationTable`

### `on1x-ownership.sty`
Ownership, borrow and lifetime notation.

**Commands:** `\OnxBorrow`, `\OnxLifetime`, `\OnxMutable`, `\OnxOwned`
**Environments:** —

### `on1x-proofs.sty`
Proof blocks and explicitly marked proof sketches.

**Commands:** `\OnxQed`
**Environments:** `OnxProofSketch`

### `on1x-semantics.sty`
Operational and denotational semantic notation.

**Commands:** `\OnxClosure`, `\OnxEnvironment`, `\OnxMeaning`, `\OnxStore`, `\OnxTransition`
**Environments:** —

### `on1x-theorems.sty`
Chapter-scoped theorem and lemma counters.

**Commands:** —
**Environments:** `OnxCorollary`, `OnxLemma`, `OnxTheorem`

### `on1x-types.sty`
Type theory and type constructor notation.

**Commands:** `\OnxArrowType`, `\OnxForall`, `\OnxProductType`, `\OnxSumType`, `\OnxType`
**Environments:** —

## Foundation

### `on1x-core.sty`
Shared metadata, author-facing notation, and theme colors.

**Commands:** `\OnxBookAuthor`, `\OnxBookDate`, `\OnxBookTitle`, `\OnxEmphasis`, `\OnxFile`, `\OnxLanguage`, `\OnxMeta`, `\OnxMuLIB`, `\OnxSetAccent`, `\OnxTerm`
**Environments:** —

### `on1x-layout.sty`
Print-sized geometry and text block controls.

**Commands:** `\OnxRestoreLayout`, `\OnxWideLayout`
**Environments:** —

### `on1x-palette.sty`
Additional semantic color roles.

**Commands:** `\OnxColorSwatch`
**Environments:** —

### `on1x-typography.sty`
Serif text with microtypography and mono code settings.

**Commands:** `\OnxSpaciousText`, `\OnxTightText`, `\OnxUI`
**Environments:** —

## Frontmatter

### `on1x-appendices.sty`
Appendix heading and entry helper.

**Commands:** `\OnxAppendixChapter`, `\OnxBeginAppendices`
**Environments:** —

### `on1x-chapters.sty`
Styled chapter openings.

**Commands:** `\OnxChapterLead`
**Environments:** —

### `on1x-copyright.sty`
Verso imprint and licensing block.

**Commands:** `\OnxCopyrightPage`, `\OnxISBN`
**Environments:** —

### `on1x-dropcaps.sty`
Decorative opening initial.

**Commands:** `\OnxDropcap`
**Environments:** —

### `on1x-epigraphs.sty`
Chapter epigraphs with source attribution.

**Commands:** `\OnxEpigraph`
**Environments:** —

### `on1x-frontmatter.sty`
Half-title and compact foreword commands.

**Commands:** `\OnxHalfTitle`
**Environments:** `OnxForeword`

### `on1x-headers.sty`
Running chapter and section headers.

**Commands:** `\OnxHeaderTitle`
**Environments:** —

### `on1x-pagination.sty`
Roman/Arabic page styles and intentional page breaks.

**Commands:** `\OnxArabicPages`, `\OnxBlankPage`, `\OnxRomanPages`
**Environments:** —

### `on1x-parts.sty`
Decorated part divider pages.

**Commands:** `\OnxPartPreface`
**Environments:** —

### `on1x-sections.sty`
Section and subsection typography.

**Commands:** `\OnxRuninHeading`
**Environments:** —

### `on1x-titlepage.sty`
Formal book cover/title spread.

**Commands:** `\OnxTitlePage`
**Environments:** —

## Language

### `on1x-backends.sty`
Backend support and lowering summaries.

**Commands:** `\OnxBackend`, `\OnxLowering`, `\OnxTargetTriple`
**Environments:** —

### `on1x-compiler.sty`
Compiler passes and phase descriptions.

**Commands:** `\OnxPass`, `\OnxPassOrder`, `\OnxPhase`
**Environments:** —

### `on1x-concurrency.sty`
Concurrency operation labels and channel diagrams.

**Commands:** `\OnxChannel`, `\OnxHappensBefore`, `\OnxReceive`, `\OnxSend`, `\OnxTask`
**Environments:** —

### `on1x-ffi.sty`
Foreign-function interface and ABI annotations.

**Commands:** `\OnxABI`, `\OnxFFIBoundary`, `\OnxForeignSymbol`
**Environments:** —

### `on1x-grammar.sty`
Long-form grammar productions.

**Commands:** `\OnxProduction`
**Environments:** `OnxGrammar`

### `on1x-ir.sty`
Intermediate representation nomenclature.

**Commands:** `\OnxBasicBlock`, `\OnxHIR`, `\OnxIRNode`, `\OnxMIR`, `\OnxSSA`
**Environments:** —

### `on1x-kernels.sty`
MuLIB kernel reference entries.

**Commands:** `\OnxKernel`, `\OnxKernelAdmission`
**Environments:** `OnxKernelCard`

### `on1x-lexical.sty`
Token descriptors and lexer rules.

**Commands:** `\OnxTokenRow`
**Environments:** `OnxTokens`

### `on1x-modules.sty`
Module graphs and import terminology.

**Commands:** `\OnxImport`, `\OnxModuleBoundary`, `\OnxModuleName`
**Environments:** —

### `on1x-ppx.sty`
PPX/extension syntax presentation.

**Commands:** `\OnxPPX`
**Environments:** `OnxExtension`

### `on1x-protocols.sty`
Protocol descriptions and wire formats.

**Commands:** `\OnxMessage`, `\OnxWireField`
**Environments:** `OnxProtocol`

### `on1x-stdlib.sty`
Standard library API callouts.

**Commands:** `\OnxLibrary`, `\OnxStdlibPath`
**Environments:** `OnxLibraryCard`

### `on1x-syntax.sty`
Inline literals, metavariables, and syntax classifications.

**Commands:** `\OnxKeyword`, `\OnxLiteral`, `\OnxNonterminal`, `\OnxOptional`, `\OnxRepeat`, `\OnxTerminal`
**Environments:** —

## Navigation

### `on1x-bibliography.sty`
Natbib citations and source bibliography.

**Commands:** `\OnxBibliography`, `\OnxCite`, `\OnxCiteText`
**Environments:** —

### `on1x-bookmarks.sty`
PDF metadata and link appearance.

**Commands:** `\OnxPDFMetadata`
**Environments:** —

### `on1x-glossary.sty`
Glossary of terms and acronyms.

**Commands:** `\OnxAcronym`, `\OnxEnableGlossary`, `\OnxGloss`, `\OnxGlossaryEntry`, `\OnxPrintGlossary`
**Environments:** —

### `on1x-hyperlinks.sty`
Named external resources and link labels.

**Commands:** `\OnxLink`, `\OnxRepoLink`, `\OnxURL`
**Environments:** —

### `on1x-index.sty`
Keyword and symbol indices.

**Commands:** `\OnxIndex`, `\OnxIndexOnly`, `\OnxPrintIndex`
**Environments:** —

### `on1x-references.sty`
Reference labels and section-aware cross-referencing.

**Commands:** `\OnxPageRef`, `\OnxRef`
**Environments:** —

### `on1x-toc.sty`
Navigation in the table of contents.

**Commands:** `\OnxContents`
**Environments:** —

## Pedagogy

### `on1x-assumptions.sty`
Explicit assumptions of a claim.

**Commands:** —
**Environments:** `OnxAssumptions`

### `on1x-boxes.sty`
Common styling for textbook panels.

**Commands:** —
**Environments:** `OnxPanel`

### `on1x-caveats.sty`
Scope restrictions and known limitations.

**Commands:** —
**Environments:** `OnxCaveat`

### `on1x-checklists.sty`
Review and implementation checklists.

**Commands:** `\OnxDone`
**Environments:** `OnxChecklist`

### `on1x-design-notes.sty`
Design alternative with rationale.

**Commands:** —
**Environments:** `OnxDesignNote`

### `on1x-difficulty.sty`
Difficulty indicators for learning tasks.

**Commands:** `\OnxDifficulty`, `\OnxPrerequisite`
**Environments:** —

### `on1x-examples.sty`
Numbered worked-example panels.

**Commands:** —
**Environments:** `OnxExample`

### `on1x-exercises.sty`
Chapter-numbered exercises with difficulty tags.

**Commands:** —
**Environments:** `OnxExercise`

### `on1x-insights.sty`
Highlighted conceptual insight.

**Commands:** —
**Environments:** `OnxInsights`

### `on1x-intuition.sty`
Intuitive explanation preceding a proof.

**Commands:** —
**Environments:** `OnxIntuition`

### `on1x-lists.sty`
Compact pedagogical and step-by-step list styles.

**Commands:** —
**Environments:** `OnxCompactItems`, `OnxSteps`

### `on1x-notes.sty`
An aside or explanatory note.

**Commands:** —
**Environments:** `OnxNote`

### `on1x-objectives.sty`
Chapter learning objectives.

**Commands:** —
**Environments:** `OnxObjectives`

### `on1x-performance.sty`
Optimization and performance discussion.

**Commands:** —
**Environments:** `OnxPerformance`

### `on1x-pitfalls.sty`
Frequent language or compiler mistake.

**Commands:** —
**Environments:** `OnxPitfall`

### `on1x-portability.sty`
Target portability caveat.

**Commands:** —
**Environments:** `OnxPortability`

### `on1x-projects.sty`
Capstone projects with a deliverable rubric.

**Commands:** `\OnxAcceptance`, `\OnxDeliverable`
**Environments:** `OnxProject`

### `on1x-questions.sty`
Open-ended questions for readers.

**Commands:** `\OnxDiscussion`
**Environments:** —

### `on1x-quizzes.sty`
Short checkpoint quizzes and multiple-choice answers.

**Commands:** `\OnxChoice`, `\OnxQuestion`
**Environments:** `OnxQuiz`

### `on1x-rubrics.sty`
Explicit assessment criteria tables.

**Commands:** `\OnxRubricRow`
**Environments:** `OnxRubric`

### `on1x-security.sty`
Security-sensitive consideration.

**Commands:** —
**Environments:** `OnxSecurity`

### `on1x-solutions.sty`
Solutions and answer-key sections.

**Commands:** `\OnxSolutionRef`
**Environments:** `OnxSolution`

### `on1x-summaries.sty`
Chapter summaries and highlights.

**Commands:** —
**Environments:** `OnxSummary`

### `on1x-tips.sty`
Practical author or programmer tips.

**Commands:** —
**Environments:** `OnxTip`

### `on1x-warnings.sty`
An explicit trap or correctness warning.

**Commands:** —
**Environments:** `OnxWarning`

## Visuals

### `on1x-algorithms.sty`
Pseudocode algorithm floats.

**Commands:** `\OnxAlgoInput`, `\OnxAlgoOutput`
**Environments:** `OnxAlgorithm`

### `on1x-ast.sty`
Abstract syntax tree visualization helpers.

**Commands:** —
**Environments:** `OnxAST`

### `on1x-automata.sty`
Finite state diagram shapes and transition edges.

**Commands:** `\OnxState`, `\OnxTransitionArrow`
**Environments:** `OnxAutomaton`

### `on1x-diagrams.sty`
TikZ defaults for compiler and architecture diagrams.

**Commands:** `\OnxDiagramArrow`, `\OnxDiagramNode`
**Environments:** `OnxDiagram`

### `on1x-figures.sty`
Figure captions, panels, and source acknowledgements.

**Commands:** `\OnxFigureSource`, `\OnxImage`
**Environments:** —

### `on1x-flowcharts.sty`
Decision and procedure flowchart nodes.

**Commands:** `\OnxDecision`, `\OnxFlowArrow`, `\OnxProcess`
**Environments:** `OnxFlowchart`

### `on1x-graphs.sty`
Labeled dependency and dataflow edges.

**Commands:** `\OnxGraphEdge`, `\OnxGraphEdgeLabel`, `\OnxGraphNode`
**Environments:** —

### `on1x-longtables.sty`
Paginated tables for language or API catalogs.

**Commands:** `\OnxRepeatHeader`
**Environments:** `OnxLongTable`

### `on1x-pipelines.sty`
A vertical compiler pipeline diagram.

**Commands:** `\OnxPipelineStage`
**Environments:** `OnxPipeline`

### `on1x-tables.sty`
Booktabs + flexible tables for technical reference.

**Commands:** `\OnxTableHead`
**Environments:** `OnxTable`

