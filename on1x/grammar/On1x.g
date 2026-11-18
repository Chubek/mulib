%{
#include "on1x_ast.h"
#include "on1x/generated_scan.h"
#include "on1x/literal_scan.h"
#include <string.h>
static on1x_arena *on1x_scanner_arena;
static const char *on1x_backtick_error;
static const char *on1x_literal_error;
static void glr_skip(GLRParser *parser);
static on1x_token_t on1x_complex_literal(GLRParser start, GLRParser *end, int kind) {
  size_t available, length, prefix;
  glr_skip(&start);
  available = (size_t)(start.end - start.s);
  prefix = start.s[0] == 'b' ? 2 : 0;
  length = kind == 1 ? on1x_lit_character(start.s + prefix, available - prefix)
                     : on1x_lit_string(start.s, available, 0);
  if (!length) {
    if (!on1x_literal_error) on1x_literal_error = start.s;
    /* Stop on the first malformed complex token without publishing tokens. */
    end->s = end->end;
    return NULL;
  }
  if (kind == 1) length += prefix;
  end->s = start.s + length;
  return on1x_Tok(on1x_scanner_arena,
      on1x_string_copy(on1x_scanner_arena, start.s, length),
      (size_t)(start.s - start.begin), on1x_token_none());
}
static on1x_token_t on1x_lexeme(char *text, GLRParser start) {
  size_t length = strlen(text);
  /* Actions run after trailing layout has been skipped. save is the state
   * BEFORE the terminal, not its end; skip only its leading layout. */
  glr_skip(&start);
  /* The scannerless parser must not reinterpret an unterminated triple opener
   * as an empty single-backtick token. The triple regexp is tried first, so
   * this branch is reached at a triple prefix only when no closing triple
   * delimiter exists. Record the lexical error while allowing the generated
   * parser to finish without publishing its speculative tokens. */
  if (!on1x_backtick_error && length == 2 && text[0] == '`' &&
      start.s + 2 < start.end && start.s[1] == '`' && start.s[2] == '`')
    on1x_backtick_error = start.s;
  on1x_token_t tok = on1x_Tok(on1x_scanner_arena,
      on1x_string_copy(on1x_scanner_arena, text, length),
      (size_t)(start.s - start.begin), on1x_token_none());
  aurocks_free(text);
  return tok;
}
%}
%%
/* The generated component is the token arena/scanner. Executable source
 * productions live in src/parser.cpp and bridge to include/on1x/ast.hpp:
 *   loop_stmt := [identifier ':'] ('loop' block | 'while' expr block
 *              | 'for' '(' [clause] ';' [expr] ';' [step] ')' block
 *              | 'for' identifier 'in' expr ('..' | '..=') expr block)
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
 * Ellipsis keeps its longest-match macro meaning. */
%start on1x_scan_unit
%skip /[ \t\r\n]+/
on1x_scan_unit: pieces {
  $$ = on1x_make_unit(on1x_scanner_arena,
      $1 ? on1x_token_some($1) : on1x_token_none());
};
pieces: %empty { $$ = NULL; }
  | pieces piece {
      on1x_token_t t = $2;
      if (t) t->v.Tok.previous = $1 ? on1x_token_some($1) : on1x_token_none();
       $$ = t ? t : $1;
    };
piece:
    /```([^`]|`[^`]|``[^`])*```/ { $$ = on1x_lexeme($1, save); }
  | /`([^`\\\r\n]|\\[^\r\n])*`/ { $$ = on1x_lexeme($1, save); }
  | /b!#\\|#\\/ { $$ = on1x_complex_literal(save, &attempt, 1); }
  | /#[^\n]*/ { $$ = NULL; }
  | /[br]!"|"/ { $$ = on1x_complex_literal(save, &attempt, 0); }
  | /[A-Za-z_][A-Za-z0-9_]*/ { $$ = on1x_lexeme($1, save); }
  | /([0-9]+\.[0-9]+([eE][+\-]?[0-9]+)?|[0-9]+[eE][+\-]?[0-9]+)(f32|f64)?|[0-9]+(f32|f64)/ { $$ = on1x_lexeme($1, save); }
  | /(0[xX][0-9a-fA-F]+|0[bB][01]+|0[oO][0-7]+|[0-9]+)(u8|u16|u32|u64)?/ { $$ = on1x_lexeme($1, save); }
  | /\.\.\.|\.\.=|\.\.|::|:=|==|!=|<=|>=|&&|\|\||<-|->|=>/ { $$ = on1x_lexeme($1, save); }
  | /[+*%<>=!;:,(){}\[\]@$.|?~\-]/ { $$ = on1x_lexeme($1, save); }
  | '/' { $$ = on1x_lexeme($1, save); }
;
%%
/* Aurocks parsers have process-wide parsing state. The C++ driver bounds and
 * validates the source, serializes calls, and copies tokens out of this arena. */
int on1x_generated_scan(const char *input, size_t length, on1x_token_callback cb, void *context,
                         size_t *error_offset) {
  on1x_arena arena;
  on1x_unit_t root;
  GLRParser parser;
  void *value = NULL;
  int result = 0;
  on1x_arena_init(&arena);
  on1x_scanner_arena = &arena;
  on1x_backtick_error = NULL;
  on1x_literal_error = NULL;
  parser.begin = input;
  parser.s = input;
  parser.end = input + length;
  parser.error = NULL;
  glr_error_pos = NULL;
  glr_depth = 0;
  root = glr_on1x_scan_unit(&parser, &value) ? value : NULL;
  glr_skip(&parser);
  if (!root || parser.s != parser.end || arena.error || on1x_backtick_error || on1x_literal_error) {
    const char *where = on1x_backtick_error;
    if (on1x_literal_error && (!where || on1x_literal_error < where)) where = on1x_literal_error;
    if (!where) {
      where = parse_on1x_scan_unit_error();
      if (!where || (parser.s != parser.end && parser.s > where)) where = parser.s;
    }
    if (error_offset) *error_offset = where ? (size_t)(where - input) : 0;
    result = arena.error ? 2 : 1;
  } else {
    on1x_token_t t;
    for (t = root->last.value; t; t = t->v.Tok.previous.value)
      cb(context, t->v.Tok.text.data, t->v.Tok.offset);
  }
  on1x_scanner_arena = NULL;
  on1x_backtick_error = NULL;
  on1x_literal_error = NULL;
  on1x_arena_destroy(&arena);
  return result;
}
