# CSS integration status — P1

The two checked-in CSS paths are separate. Neither is a native browser style
or rendering engine.

| Kernel family | Verified bounded profile and next work |
| --- | --- |
| [`css-tokenizer`](mulib-css-tokenizer.md), [`css-parser`](mulib-css-parser.md) | Borrowed lexical spans and owned flat rules, respectively. The parser does its own lexical work; it does not consume tokenizer events. Full tokenization and error recovery, at-rules and nested rules remain open. |
| [`css-selector`](mulib-css-selector.md) | Simple type, ID, class and universal matching via DOM query on `mulib_html_tree_node_t`. No native DOM adapter, combinators, selector lists or pseudo syntax. |
| [`css-cascade`](mulib-css-cascade.md), [`css-computed`](mulib-css-computed.md) | Five computed fields and explicit inheritance of color/font-size. Matching author declarations use importance, simple specificity and source order per property. Checked by `tests/test_web_css.c`. No inline styles, layers, multiple origins, general inheritance or resolved custom properties. |
| [`css`](mulib-css.md), [`stylesheet`](mulib-stylesheet.md) | Independent XML-node path with compound selectors, child/descendant matching, selector lists and author/inline precedence. `stylesheet` adds its own selected typed properties. Not interchangeable with the HTML-tree or native DOM representations. |
| [`css-box`](mulib-css-box.md), [`css-flex`](mulib-css-flex.md), [`css-grid`](mulib-css-grid.md), [`css-layoutstream`](mulib-css-layoutstream.md) | Nonvisual vertical placement, weighted one-axis distribution, uniform cells and synchronous index/box callbacks. These are not document layout, Flexbox/Grid formatting contexts or a renderer stream. |

## Staging inventory disposition

The CSS entries of the staging manifest already have headers in this
checkout. They remain separate static-inline helpers, not new dependencies
of the HTML-tree cascade, and were not behaviorally reverified for this
slice. The following rows track their remaining integration work.

| Staging CSS rows (`mulib-*.h`) | Not supplied by this slice |
| --- | --- |
| `css-values`, `css-units`, `css-functions`, `css-properties`, `css-shorthand` | Property grammar, typed computed-value integration and relative unit resolution. |
| `css-customprops`, `css-invalidation`, `css-pseudoelement` | `var()` substitution, cascade/inheritance, dependency tracking and generated content. |
| `css-mediaquery`, `css-containerquery`, `css-supports` | Condition parsing and reevaluation. The container helper already filters a supplied ancestor list by name and required axes; it is not containment or style-query evaluation. |
| `cssom`, `cssom-style`, `cssom-typed` | Live CSSOM, rule/element bindings, serialization and type algebra. |
| `css-fontface`, `css-inline`, `css-block`, `css-position`, `css-overflow`, `css-intrinsic` | Font loading/measurement, formatting contexts, containing blocks, clipping and intrinsic layout. |
| `css-transform`, `css-stacking`, `css-animation`, `css-transition`, `css-hittest` | Integrated geometry and stacking contexts, animation timelines, painting and event targeting. |

Native style resolution needs the native DOM element/attribute/traversal and
mutation contracts; no second tree or guessed adapter was added. Publishing
computed style and geometry needs the browser stream owner's typed schemas
and ID/lifetime contracts. Neither dependency is required to use the committed
HTML-tree author-cascade subset. No WPT or browser differential result is claimed.
