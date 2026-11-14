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
  | /0[xX][0-9a-fA-F]+|[0-9]+/ { $$ = on1x_lexeme($1, save); }
  | /\.\.\.|==|!=|<=|>=|&&|\|\||->|=>/ { $$ = on1x_lexeme($1, save); }
  | /[+*%<>=!;:,(){}\[\]@$.\-]/ { $$ = on1x_lexeme($1, save); }
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
