%{
#include "on1x_ast.h"
#include "on1x/generated_scan.h"
#include "on1x/literal_scan.h"
#include "on1x/operator_scan.h"
#include <stdlib.h>
#include <string.h>
%}
/* The generated component is the token arena/scanner. Executable source
 * productions live in src/parser.cpp and bridge to include/on1x/ast.hpp:
 *   module_header := 'module' qualified_identifier ';'     (.on1xm, first item)
 *   abstract_header := 'abstract' 'module' qualified_identifier ';' (.on1xi)
 *   module_import := 'import' qualified_identifier import_scope ';'
 *                  | 'from' module_path 'import' selectors ';'
 *   program_import := 'import' 'program' (qualified_identifier | quoted_path)
 *                     import_scope ';'
 *   import_scope := empty | 'as' identifier | ':' selectors
 *   visibility := 'public' | 'private' | 'shared' ['elevate' '(' decimal ')']
 * These words are contextual. Module syntax uses ordinary identifier,
 * punctuation and number terminals. The parser dispatches the zero elevation
 * fault only for a complete shared visibility clause.
 *   foreign_import := 'import' 'foreign' 'stdffi' '.' identifier
 *                     ['as' identifier] ';'
 *   kernel_import := ['unsafe'] 'import' 'kernel' ['[' 'dynlib' ']']
 *                    qualified_identifier ['as' identifier] ';'
 * `foreign`, `kernel`, and `unsafe` are contextual words.
 *   loop_stmt := [identifier ':'] ('loop' block | 'while' expr block
 *              | 'for' '(' [clause] ';' [expr] ';' [step] ')' block
 *              | 'for' identifier 'in' expr ('..' | '..=') expr block
 *              | 'for' iteration_pattern 'in' expr block)
 *   iteration_pattern := identifier | '_' | '(' iteration_pattern
 *                       (',' iteration_pattern)* ')'
 *   loop_value := [identifier ':'] 'loop' block
 *   loop_exit := 'break' [identifier] ['with' expr] ';'
 *              | 'continue' [identifier] ';'
 *   match := 'match' (expr | 'let' identifier '=' expr) 'with' '{' arms '}'
 *   sum_match := expr '~' '{' arms '}'  (known closed sum; also admits '-')
 *   regex_test := ('match' | 'capture') expr '~' postfix_expr
 *   ref_parameter := identifier ':' 'ref' 'i64'
 *   ref_store := identifier ':=' expr ';'
 *   arm := pattern ['if' expr] '=>' (expr | arm_block)
 *   pattern := identifier | '_' | 'rest' | literal | constructor(parts)
 *            | (parts) | [parts] | record{fields[,..]} | pattern '|' pattern
 *            | pattern 'as' identifier
 *   nominal := ('record' | 'enum') identifier ['<' parameters '>'] '{' fields '}'
 *   named_type := qualified_identifier ['<' type_arguments '>']
 *   named_record := 'record' '<' named_type '>' '{' initializers '}'
 *   named_variant := 'variant' '<' named_type '>' '(' tag ',' payload ')'
 *   inherent_impl := 'impl' ['<' type_parameters '>'] named_type
 *                    '{' inherent_member+ '}'
 *   inherent_member := 'fn' identifier ['<' type_parameters '>']
 *                       '(' [receiver ','] parameters ')' ['->' type] block
 *                    | 'value' identifier ['(' receiver ')'] [':' type]
 *                       '=' expression ';'
 *   receiver := 'self' [':' named_type]
 *             | ('ref' | 'move') 'self' [':' named_type]
 *   member_access := postfix_expression '::' identifier
 *                  | named_type '::' identifier
 * `ref self` and `move self` are parsed for stable diagnostics but are not
 * admitted until place/lifetime and consuming-ownership lowering exists.
 * Nonrecursive nominal declarations use existing bounded by-value data layouts.
 *   defer_stmt := 'defer' call_expression ';'  (function-scoped, Go-style)
 *   propagation := postfix_expression '?'     (nominal typed Result return)
 * Loop/match/impl/value words are contextual. `::` is the inherent/cliche
 * member operator; it does not replace record projection or list syntax.
 * Intrinsic cooperative profile: taskgroup ['within' expression ['ms'|'s']] block,
 * launch block, join/split/
 * close prefix, <-receive and <-send, ->(1,1){payload,capacity,mode:uni},
 * and contextual poll {recv/send/closed/after/default => arm}.
 * POSIX bounded profile: launch exec("/absolute/path", "literal-arg", ...)
 * or launch [capture] `absolute argv words`; capture binds process/stdin/
 * stdout/stderr capabilities. The parser, not a host shell, decodes words.
 *   operator_decl := 'operator' ['<' type_parameters '>']
 *                    '(' attributes ', formals:' parameter_list ')'
 *                    ['->' type] ['where' constraints] block
 *   operator_ref := 'operator' '(' quoted_operator ')'
 *   fixity := ('infix' | 'infixr') decimal_tightness operator_token ';'
 * Operator declarations inside a block introduce a noncapturing callable
 * and a syntax binding valid from the declaration through the end of that
 * block. Module-scope headers are visible throughout their module.
 * Custom punctuation uses the shared bounded recognizer before ordinary
 * punctuation; ellipsis keeps its longest-match macro meaning. */
%backend portable
%start on1x_scan_terminal
%skip /[ \t\r\n]+/

/* Opt-in scanner faults, anchored at the first failed terminal. The two
 * banks contain 60 rules each; keep the bank markers on their own lines.
 * The callback preserves a token boundary for a UTF-8 BOM away from byte zero. */
%fault "'\\xEF\\xBB\\xBF'" => silent_patch "" using on1x_fault_hint
%fault "'\\xFF\\xFE'" => error "UTF-16 little-endian signature; save On1x source as UTF-8" using on1x_fault_hint
%fault "'\\xFE\\xFF'" => error "UTF-16 big-endian signature; save On1x source as UTF-8" using on1x_fault_hint
%fault "'\\x0E\\xFE\\xFF'" => error "SCSU signature; save On1x source as UTF-8" using on1x_fault_hint
%fault "'\\xFB\\xEE\\x28'" => error "BOCU-1 signature; save On1x source as UTF-8" using on1x_fault_hint
%fault "'\\x84\\x31\\x95\\x33'" => error "GB18030 signature; save On1x source as UTF-8" using on1x_fault_hint
%fault "'\\xDD\\x73\\x66\\x73'" => error "UTF-EBCDIC signature; save On1x source as UTF-8" using on1x_fault_hint
%fault "'\\xF7\\x64\\x4C'" => error "UTF-1 signature; save On1x source as UTF-8" using on1x_fault_hint

/* These ASCII faults start with an unsupported byte. Operators with only
 * valid ASCII token bytes (such as +=) belong to parser diagnostics. */
%fault "'\\\\' [\\r\\n]" => inform "On1x does not use backslash line continuations" using on1x_fault_hint
%fault "'\\\\' [A-Za-z]" => inform "backslash names are not On1x tokens; use an identifier without a backslash" using on1x_fault_hint
%fault "'\\\\'" => error "backslash is not a standalone On1x token" using on1x_fault_hint
%fault "'&='" => error "On1x has no &= operator" using on1x_fault_hint
%fault "'&'" => inform "ampersand is not a standalone On1x token; use && for logical conjunction" using on1x_fault_hint
%fault "'^='" => error "On1x has no ^= operator" using on1x_fault_hint
%fault "'^'" => error "caret is not an On1x operator" using on1x_fault_hint

/* Foreign operator spellings are suggestions, never edits. */
%fault "'≔'" => inform "use := for an On1x reference store" using on1x_fault_hint
%fault "'≕'" => error "reversed assignment is not an On1x operator" using on1x_fault_hint
%fault "'≡'" => warn "On1x has no identity glyph operator; check whether value equality (==) is intended" using on1x_fault_hint
%fault "'≈'" => error "On1x has no approximate-equality operator; express the tolerance explicitly" using on1x_fault_hint
%fault "'≢'" => inform "use != to test inequality; On1x has no non-identity glyph operator" using on1x_fault_hint
%fault "'∈'" => inform "use the keyword in for a for-loop iteration clause" using on1x_fault_hint
%fault "'∉'" => error "On1x has no not-a-member glyph operator" using on1x_fault_hint
%fault "'∧'" => inform "use && for logical conjunction" using on1x_fault_hint
%fault "'∨'" => inform "use || for logical disjunction" using on1x_fault_hint
%fault "'¬'" => inform "use ! for logical negation" using on1x_fault_hint
%fault "'∷'" => inform "use :: for a qualified member; the proportion sign is not an operator" using on1x_fault_hint
%fault "'∅'" => error "the empty set symbol is not an On1x literal" using on1x_fault_hint
%fault "'∞'" => error "the infinity symbol is not an On1x numeric literal" using on1x_fault_hint
%fault "'×'" => inform "use * for multiplication" using on1x_fault_hint
%fault "'÷'" => inform "use / for division" using on1x_fault_hint
%fault "'→'" => inform "use -> for an On1x return-type or channel arrow" using on1x_fault_hint
%fault "'←'" => inform "use <- for an On1x channel arrow" using on1x_fault_hint
%fault "'⇒'" => inform "use => for an On1x match or poll arm" using on1x_fault_hint
%fault "'≤'" => inform "use <= for a less-than-or-equal comparison" using on1x_fault_hint
%fault "'≥'" => inform "use >= for a greater-than-or-equal comparison" using on1x_fault_hint
%fault "'≠'" => inform "use != for inequality" using on1x_fault_hint
%fault "'−'" => inform "use an ASCII hyphen-minus for subtraction or a negative number" using on1x_fault_hint
%fault "'±'" => error "plus-minus is not an On1x operator; write the intended operation explicitly" using on1x_fault_hint
%fault "'…'" => inform "use ASCII dots: .. or ..= for a range, or ... where an ellipsis is required" using on1x_fault_hint

/* Re-spell only exact fullwidth forms of standalone structural punctuation.
 * Never create a comment/documentation opener by replacing an opening brace. */
%fault "'；'" => loud_patch ";" "replace fullwidth semicolon with ASCII ;" using on1x_fault_hint
%fault "'，'" => loud_patch "," "replace fullwidth comma with ASCII ," using on1x_fault_hint
%fault "'（'" => loud_patch "(" "replace fullwidth opening parenthesis with ASCII (" using on1x_fault_hint
%fault "'）'" => loud_patch ")" "replace fullwidth closing parenthesis with ASCII )" using on1x_fault_hint
%fault "'［'" => loud_patch "[" "replace fullwidth opening square bracket with ASCII [" using on1x_fault_hint
%fault "'］'" => loud_patch "]" "replace fullwidth closing square bracket with ASCII ]" using on1x_fault_hint
%fault "'｛' ![-@!>]" => loud_patch "{" "replace fullwidth opening brace with ASCII {" using on1x_fault_hint
%fault "'｝'" => loud_patch "}" "replace fullwidth closing brace with ASCII }" using on1x_fault_hint

/* The scanner has already rejected these at their opener. Do not insert a
 * closer: where one belongs cannot be inferred from a lexical failure. */
%fault "'{-'" => error "invalid or unterminated {- comment; check nesting and -} closers" using on1x_fault_hint
%fault "'{@'" => error "invalid or unterminated {@ comment; check nesting and @} closers" using on1x_fault_hint
%fault "'{!'" => error "unterminated {! docstring; expected a closing }" using on1x_fault_hint
%fault "'{>'" => error "unterminated {> docstring; expected a closing }" using on1x_fault_hint
%fault "'b!#\\\\'" => error "missing byte character after b!#\\; expected a value immediately after the opener" using on1x_fault_hint
%fault "'#\\\\'" => error "missing character after #\\; expected a value immediately after the opener" using on1x_fault_hint
%fault "'```'" => error "unterminated triple-backtick literal; expected a closing fence" using on1x_fault_hint
%fault "'`'" => error "invalid or unterminated backtick literal; check escapes and close before the line ends" using on1x_fault_hint
%fault "([rb] '!')? ['] !\"''\"" => error "single apostrophe is not a string delimiter; use three apostrophes, double quotes, or #\\ for a character" using on1x_fault_hint
%fault "'r!' ['\"]" => error "invalid or unterminated raw string; check its closing quote delimiter" using on1x_fault_hint
%fault "'b!' ['\"]" => error "invalid or unterminated byte string; check the closing delimiter and byte escapes such as \\x41" using on1x_fault_hint
%fault "'\"\"\"' / \"'''\"" => error "invalid or unterminated multiline string; check its delimiter, escapes, and interpolation" using on1x_fault_hint
%fault "'\"'" => error "invalid or unterminated string; check closing quote, escapes, or interpolation before the line ends" using on1x_fault_hint
/* ON1X_FAULT_BANK_2 */
/* Unicode typography and lookalikes are diagnostic-only where a replacement
 * could create a different operator, literal delimiter, or comment. */
%fault "'‘' / '’'" => inform "typographic apostrophe is not an On1x quote; use ASCII literal delimiters" using on1x_fault_hint
%fault "'“' / '”'" => inform "use straight ASCII double quotes for an On1x string" using on1x_fault_hint
%fault "'«' / '»'" => inform "guillemets are not On1x delimiters; use straight quotes for strings" using on1x_fault_hint
%fault "'‹' / '›'" => inform "single guillemets are not angle brackets or quote delimiters in On1x" using on1x_fault_hint
%fault "'′' / '″' / '‴'" => inform "a prime mark is not an ASCII quote or a valid identifier suffix" using on1x_fault_hint
%fault "'＂' / '＇'" => inform "fullwidth quotes are not On1x literal delimiters" using on1x_fault_hint
%fault "'｀'" => inform "use ASCII backticks for a backtick literal" using on1x_fault_hint
%fault "'「' / '」' / '『' / '』'" => inform "corner quotes are not On1x delimiters; use ASCII literal delimiters" using on1x_fault_hint
%fault "'｛'" => warn "fullwidth opening brace was not repaired because an ASCII brace here would introduce a comment or docstring" using on1x_fault_hint
%fault "'：'" => inform "fullwidth colon is not a token; use ASCII :, ::, or := as appropriate" using on1x_fault_hint
%fault "'．'" => inform "fullwidth full stop is not a token; use an ASCII dot for projection, decimals, or ranges" using on1x_fault_hint
%fault "'！'" => inform "fullwidth exclamation is not a token; check ASCII !, !=, or a literal prefix" using on1x_fault_hint
%fault "'？'" => inform "use ASCII ? for postfix Result propagation" using on1x_fault_hint
%fault "'％'" => inform "use ASCII % for remainder" using on1x_fault_hint
%fault "'＋'" => inform "use ASCII + for addition" using on1x_fault_hint
%fault "'－'" => inform "fullwidth hyphen-minus is not a token; use ASCII - where intended" using on1x_fault_hint
%fault "'＊'" => inform "use ASCII * for multiplication, not a fullwidth asterisk" using on1x_fault_hint
%fault "'／'" => inform "use an ASCII slash for division or a regex delimiter as appropriate" using on1x_fault_hint
%fault "'＝'" => inform "fullwidth equals is not a token; check whether = or == is intended" using on1x_fault_hint
%fault "'＜' / '＞'" => inform "use ASCII angle signs for comparisons or type parameters" using on1x_fault_hint
%fault "'｜'" => inform "use ASCII | for pattern alternatives or || for logical disjunction" using on1x_fault_hint
%fault "'＃'" => inform "fullwidth number sign does not open a comment or character literal" using on1x_fault_hint
%fault "'＠'" => inform "fullwidth at sign is not the ASCII @ used for annotations or invocations" using on1x_fault_hint
%fault "'＄'" => inform "fullwidth dollar sign is not an ASCII $ token" using on1x_fault_hint
%fault "'～'" => inform "use ASCII ~ for a match connective" using on1x_fault_hint
%fault "'＿'" => inform "use ASCII _ in an identifier or wildcard" using on1x_fault_hint
%fault "'、'" => inform "ideographic comma is not a separator; use ASCII comma" using on1x_fault_hint
%fault "'。'" => inform "ideographic full stop is not a token; check the intended ASCII punctuation" using on1x_fault_hint
%fault "'·' / '・'" => inform "middle dot is not an On1x operator, separator, or identifier character" using on1x_fault_hint
%fault "'•'" => inform "list bullets are not On1x syntax; check for pasted formatted text" using on1x_fault_hint
%fault "'‒' / '–' / '—' / '―'" => inform "a typographic dash is not the ASCII hyphen-minus used in On1x" using on1x_fault_hint
%fault "'∕' / '⁄'" => inform "division and fraction slashes are not ASCII slash tokens" using on1x_fault_hint
%fault "'∗' / '⋆'" => inform "use ASCII * for multiplication, not a mathematical asterisk or star" using on1x_fault_hint
%fault "'⋅' / '∙'" => inform "dot-product glyph is not an On1x operator; express the operation explicitly" using on1x_fault_hint
%fault "'〈' / '〉' / '⟨' / '⟩'" => inform "mathematical or CJK angle brackets are not ASCII type delimiters" using on1x_fault_hint
%fault "'【' / '】' / '〔' / '〕'" => inform "ornamental brackets are not On1x delimiters" using on1x_fault_hint
%fault "';'" => warn "Greek question mark looks like a semicolon but is not a statement terminator" using on1x_fault_hint
%fault "'„' / '‟'" => inform "curved double quotes are not On1x string delimiters" using on1x_fault_hint
%fault "'ʼ' / 'ʻ'" => inform "modifier-letter apostrophe is not an ASCII quote or identifier character" using on1x_fault_hint

/* Invisible characters and invalid UTF-8: diagnose, never delete. */
%fault "'\\xC2\\xA0' / '\\xE2\\x80\\xAF'" => warn "non-breaking space outside a literal; use ASCII space where intended" using on1x_fault_hint
%fault "'\\xE1\\x9A\\x80' / '\\xE2\\x80' [\\x80-\\x8A] / '\\xE2\\x81\\x9F' / '\\xE3\\x80\\x80'" => warn "Unicode space outside a literal; use ASCII layout characters" using on1x_fault_hint
%fault "'\\xE2\\x80\\x8B'" => warn "zero-width space outside a literal" using on1x_fault_hint
%fault "'\\xE2\\x80' [\\x8C-\\x8D]" => warn "zero-width joiner or non-joiner outside a literal" using on1x_fault_hint
%fault "'\\xE2\\x81\\xA0'" => warn "invisible word joiner outside a literal" using on1x_fault_hint
%fault "'\\xC2\\xAD'" => warn "invisible soft hyphen outside a literal" using on1x_fault_hint
%fault "'\\xE1\\xA0\\x8E'" => warn "invisible Mongolian vowel separator outside a literal" using on1x_fault_hint
%fault "'\\xE2\\x80' [\\x8E-\\x8F] / '\\xD8\\x9C'" => warn "bidirectional direction mark outside a literal" using on1x_fault_hint
%fault "'\\xE2\\x80' [\\xAA-\\xAE]" => warn "bidirectional embedding, override, or pop control outside a literal" using on1x_fault_hint
%fault "'\\xE2\\x81' [\\xA6-\\xA9]" => warn "bidirectional isolate control outside a literal" using on1x_fault_hint
%fault "'\\xE2\\x80' [\\xA8-\\xA9]" => warn "Unicode line or paragraph separator; use an ASCII newline" using on1x_fault_hint
%fault "'\\xC2\\x85'" => warn "Unicode next-line control; use an ASCII newline" using on1x_fault_hint
%fault "'\\v'" => warn "vertical tab is not On1x layout; use a space, tab, or newline" using on1x_fault_hint
%fault "'\\f'" => warn "form feed is not On1x layout; use an ASCII newline" using on1x_fault_hint
%fault "'\\x1B'" => error "escape control outside a literal; check for pasted terminal control sequences" using on1x_fault_hint
%fault "'\\x7F'" => error "ASCII delete control outside a literal" using on1x_fault_hint
%fault "[\\x01-\\x08\\x0E-\\x1A\\x1C-\\x1F]" => error "ASCII control byte outside a literal" using on1x_fault_hint
%fault "'\\xC2' [\\x80-\\x9F]" => error "Unicode C1 control outside a literal" using on1x_fault_hint
%fault "[\\x80-\\xBF\\xC0-\\xC1\\xF5-\\xFF]" => error "invalid leading byte for a UTF-8 character" using on1x_fault_hint
%fault "[\\xC2-\\xDF] [\\x80-\\xBF] / ('\\xE0' [\\xA0-\\xBF] / [\\xE1-\\xEC\\xEE-\\xEF] [\\x80-\\xBF] / '\\xED' [\\x80-\\x9F]) [\\x80-\\xBF] / ('\\xF0' [\\x90-\\xBF] / [\\xF1-\\xF3] [\\x80-\\xBF] / '\\xF4' [\\x80-\\x8F]) [\\x80-\\xBF] [\\x80-\\xBF]" => error "unsupported Unicode character in token position; On1x identifiers use ASCII" using on1x_fault_hint
%fault "." => error "invalid token or malformed UTF-8 in source" using on1x_fault_hint
/* ON1X_FAULT_BANK_END */
/* ON1X_CONTEXTUAL_FAULTS */
/* This bank is never used at a lexical failure. After finding a complete
 * zero-valued shared elevation clause, the parser supplies its canonical
 * spelling to the PEG and reports against the original decimal token. */
%fault "'elevate(0)' !." => error "shared elevation cannot be zero; use a positive decimal integer"
/* ON1X_CONTEXTUAL_FAULTS_END */

/* Keep alternatives and on1x_rx_rule in the same order. The ordinary
 * terminal combines numbers, identifiers and operators for maximal munch. */
on1x_scan_terminal:
    /[A-Za-z_][A-Za-z0-9_]*|([0-9]+\.[0-9]+([eE][+\-]?[0-9]+)?|[0-9]+[eE][+\-]?[0-9]+)(f32|f64)?|[0-9]+(f32|f64)|(0[xX][0-9a-fA-F]+|0[bB][01]+|0[oO][0-7]+|[0-9]+)(u8|u16|u32|u64)?|\.\.\.|\.\.=|\.\.|::|:=|==|!=|<=|>=|&&|\|\||<-|->|=>|[+*%<>=!;:,(){}\[\]@$.|?~\/\-]/
  | /#[^\n]*/
  | /`([^`\\\r\n]|\\[^\r\n])*`/
  | /```([^`]|`[^`]|``[^`])*```/
  ;
%%
#include "on1x_fault_bank_2.h"

struct on1x_fault_step {
  size_t offset;
  int attempted, matched, allocation_failed;
  char *replacement;
};

static _Thread_local struct on1x_fault_step *on1x_active_fault;

static void on1x_fault_hint(const aurocks_fault_state *state, aurocks_fault_response *response) {
  if (on1x_active_fault && state->native_parser == on1x_active_fault)
    on1x_active_fault->matched = 1;
  /* A UTF-8 BOM at the start has no token meaning. Preserve a separator
   * when the same marker interrupts source in the middle of a file. */
  if (state->offset && !*response->replacement && state->match_end - state->offset == 3 &&
      memcmp(state->input + state->offset, "\xef\xbb\xbf", 3) == 0)
    response->replacement = " ";
}

static void *on1x_fault_attempt(const char *input, const char **error,
                               aurocks_fault_observer observe, void *data, const void *unused) {
  struct on1x_fault_step *step = on1x_active_fault;
  (void)unused;
  if (!step->attempted++) {
    *error = input + step->offset;
    observe(data, AUROCKS_FAULT_EARLEY, AUROCKS_FAULT_SYNTAX, step, step->offset, NULL);
  } else {
    /* Return the first edit to the caller. The canonical lexer must rescan
     * it with fresh offsets before we dispatch on any subsequent failure. */
    const size_t length = strlen(input) + 1;
    step->replacement = malloc(length);
    if (step->replacement) memcpy(step->replacement, input, length);
    else step->allocation_failed = 1;
    *error = NULL;
  }
  return NULL;
}

int on1x_aurocks_fault_step(const char *input, size_t offset, char **replacement) {
  if (!replacement) return 0;
  *replacement = NULL;
  if (!input || offset > strlen(input)) return 0;
  struct on1x_fault_step step = {0};
  struct on1x_fault_step *previous = on1x_active_fault;
  const char *error;
  step.offset = offset;
  on1x_active_fault = &step;
  aurocks_fault_run(input, &error, on1x_fault_attempt, NULL, aurocks_fault_rules,
                    sizeof(aurocks_fault_rules) / sizeof(aurocks_fault_rules[0]));
  if (!step.matched) {
    step.attempted = 0;
    aurocks_fault_run(input, &error, on1x_fault_attempt, NULL, on1x_fault_bank_2,
                      sizeof(on1x_fault_bank_2) / sizeof(on1x_fault_bank_2[0]));
  }
  on1x_active_fault = previous;
  *replacement = step.replacement;
  return step.allocation_failed ? -1 : step.replacement != NULL;
}

/* The pinned native generator has no public prefix API. This epilogue uses
 * its emitted portable matcher, not the Earley whole-input entry point.
 * It uses the same generated expression tables without building a chart.
 * The complex terminals still use the shared bounded recognizers. */
enum on1x_rx_rule { ON1X_ORDINARY, ON1X_LINE_COMMENT, ON1X_BACKTICK,
                    ON1X_TRIPLE_BACKTICK, ON1X_RX_RULES };
_Static_assert(AUROCKS_RULES == ON1X_RX_RULES, "On1x terminal table changed");

static size_t on1x_rx_width(aurocks_parser *matcher, size_t rx, size_t pos) {
  matcher->work = 0;
  const size_t end = aurocks_rx_match(matcher, rx, pos);
  return end == (size_t)-1 ? 0 : end - pos;
}

static size_t on1x_terminal_width(aurocks_parser *matcher, size_t pos, int *trivia) {
  const char *start = matcher->input + pos;
  const size_t available = matcher->length - pos;
  const int comment = on1x_source_comment_kind(start, available);
  enum on1x_rx_rule rule = ON1X_ORDINARY;
  *trivia = 0;
  if (comment) {
    *trivia = comment == '-' || comment == '@';
    return on1x_comment_width(start, available);
  }
  if ((available >= 2 && memcmp(start, "#\\", 2) == 0) ||
      (available >= 4 && memcmp(start, "b!#\\", 4) == 0)) {
    const size_t prefix = *start == 'b' ? 2 : 0;
    const size_t width = on1x_lit_character(start + prefix, available - prefix);
    return width ? prefix + width : 0;
  }
  if (*start == '"' || *start == '\'' ||
      (available >= 3 && (*start == 'r' || *start == 'b') && start[1] == '!' &&
       (start[2] == '"' || start[2] == '\'')))
    return on1x_lit_string(start, available, 0);
  if (*start == '#') {
    rule = ON1X_LINE_COMMENT;
    *trivia = 1;
  } else if (*start == '`') {
    rule = available >= 3 && memcmp(start, "```", 3) == 0 ?
      ON1X_TRIPLE_BACKTICK : ON1X_BACKTICK;
  } else if (strchr(" \t\r\n", *start)) {
    *trivia = 1;
    return on1x_rx_width(matcher, aurocks_skip_rx, pos);
  }
  if (rule == ON1X_ORDINARY) {
    const size_t width = on1x_operator_width(start, available);
    if (width) return width;
  }
  return on1x_rx_width(matcher, aurocks_rhs[aurocks_rules[rule].offset].id, pos);
}

int on1x_generated_scan(const char *input, size_t length, on1x_token_callback cb, void *context,
                         size_t *error_offset) {
  /* The emitted matcher can inspect input[length]. The scanner's public
   * contract does not require a sentinel or allow a read past length. */
  if (length == (size_t)-1) return 2;
  char *bounded = malloc(length + 1);
  if (!bounded) return 2;
  memcpy(bounded, input, length);
  bounded[length] = '\0';

  aurocks_parser matcher = {0};
  matcher.input = bounded;
  matcher.length = length;
  on1x_arena arena;
  on1x_token_t last = NULL;
  size_t pos = 0;
  int result = 0;
  on1x_arena_init(&arena);
  while (pos < length) {
    int trivia;
    const size_t width = on1x_terminal_width(&matcher, pos, &trivia);
    if (!width || matcher.failed) {
      result = 1;
      break;
    }
    if (!trivia) {
      last = on1x_Tok(&arena, on1x_string_copy(&arena, bounded + pos, width), pos,
          last ? on1x_token_some(last) : on1x_token_none());
      if (!last || arena.error) {
        result = 2;
        break;
      }
    }
    pos += width;
  }
  if (result) {
    if (error_offset) *error_offset = pos;
  } else {
    for (on1x_token_t t = last; t; t = t->v.Tok.previous.value)
      cb(context, t->v.Tok.text.data, t->v.Tok.offset);
  }
  on1x_arena_destroy(&arena);
  free(bounded);
  return result;
}
