%{
#include "on1x_ast.h"
#include "on1x/generated_scan.h"
#include <string.h>
static on1x_arena *on1x_scanner_arena;
static void glr_skip(GLRParser *parser);
static on1x_token_t on1x_lexeme(char *text, GLRParser start) {
  size_t length = strlen(text);
  /* Actions run after trailing layout has been skipped. save is the state
   * BEFORE the terminal, not its end; skip only its leading layout. */
  glr_skip(&start);
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
 *   arm := pattern ['if' expr] '=>' (expr | arm_block)
 *   pattern := identifier | '_' | 'rest' | literal | constructor(parts)
 *            | (parts) | [parts] | record{fields[,..]} | pattern '|' pattern
 *            | pattern 'as' identifier
 *   nominal := ('record' | 'enum') identifier ['<' parameters '>'] '{' fields '}'
 *   named_type := qualified_identifier ['<' type_arguments '>']
 *   named_record := 'record' '<' named_type '>' '{' initializers '}'
 *   named_variant := 'variant' '<' named_type '>' '(' tag ',' payload ')'
 * Nonrecursive nominal declarations use existing bounded by-value data layouts.
 *   defer_stmt := 'defer' call_expression ';'  (function-scoped, Go-style)
 *   propagation := postfix_expression '?'     (nominal typed Result return)
 * Loop/match words are contextual. '::' is recognized so owning-list patterns
 * can diagnose their unsupported profile; array tails are never invented.
  * Intrinsic cooperative profile: taskgroup block, launch block, join/split/
   * close prefix, <-receive and <-send, ->(1,1){payload,capacity,mode:uni},
   * and contextual poll {recv/send/closed/default => arm}.
   * POSIX bounded profile: launch exec("/absolute/path", "literal-arg", ...)
   * yields xfiber, joined or cancelled within its taskgroup; no shell lexer.
  * Ellipsis keeps its longest-match macro meaning. */
%start on1x_scan_unit
%skip /([ \t\r\n]+|#[^\n]*)+/
on1x_scan_unit: pieces {
  $$ = on1x_make_unit(on1x_scanner_arena,
      $1 ? on1x_token_some($1) : on1x_token_none());
};
pieces: %empty { $$ = NULL; }
  | pieces piece {
      on1x_token_t t = $2;
      if (t) t->v.Tok.previous = $1 ? on1x_token_some($1) : on1x_token_none();
      $$ = t;
    };
piece:
    /"([^"\\\r\n]|\\["\\nrt0])*"/ { $$ = on1x_lexeme($1, save); }
  | /[A-Za-z_][A-Za-z0-9_]*/ { $$ = on1x_lexeme($1, save); }
  | /([0-9]+\.[0-9]+([eE][+\-]?[0-9]+)?|[0-9]+[eE][+\-]?[0-9]+)(f32|f64)?|[0-9]+(f32|f64)/ { $$ = on1x_lexeme($1, save); }
  | /(0[xX][0-9a-fA-F]+|[0-9]+)(u8|u16|u32|u64)?/ { $$ = on1x_lexeme($1, save); }
  | /\.\.\.|\.\.=|\.\.|::|==|!=|<=|>=|&&|\|\||<-|->|=>/ { $$ = on1x_lexeme($1, save); }
  | /[+*%<>=!;:,(){}\[\]@$.|?\-]/ { $$ = on1x_lexeme($1, save); }
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
  parser.begin = input;
  parser.s = input;
  parser.end = input + length;
  parser.error = NULL;
  glr_error_pos = NULL;
  glr_depth = 0;
  root = glr_on1x_scan_unit(&parser, &value) ? value : NULL;
  glr_skip(&parser);
  if (!root || parser.s != parser.end || arena.error) {
    const char *where = parse_on1x_scan_unit_error();
    if (!where || (parser.s != parser.end && parser.s > where)) where = parser.s;
    if (error_offset) *error_offset = where ? (size_t)(where - input) : 0;
    result = arena.error ? 2 : 1;
  } else {
    on1x_token_t t;
    for (t = root->last.value; t; t = t->v.Tok.previous.value)
      cb(context, t->v.Tok.text.data, t->v.Tok.offset);
  }
  on1x_scanner_arena = NULL;
  on1x_arena_destroy(&arena);
  return result;
}
