#define MULIB_IMPL_READLINE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_TERMUTILS
#include "mulib-readline.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main (int argc, char **argv)
{
  mulib_readline_t *r = mulib_readline_create (NULL);
  char *rendered = NULL;
  assert (r);
  assert (!mulib_readline_rule (r, MULIB_READLINE_WORD, MULIB_READLINE_KEYWORD, "return", NULL, 0));
  assert (!mulib_readline_completion (r, "return", "Return a result"));
  assert (!mulib_readline_set (r, "ret", 3));
  assert (!mulib_readline_accept (r, 0));
  assert (!strcmp (mulib_readline_text (r, NULL, NULL), "return"));
  assert (!mulib_readline_render (r, "> ", 80, &rendered)); free (rendered);
  if (argc > 1)
    {
      char *line = NULL;
      assert (!mulib_readline_load (r, argv[1], NULL));
      if (mulib_readline_read (r, 0, 1, "> ", &line) == MULIB_READLINE_LINE) { puts (line); free (line); }
    }
  mulib_readline_destroy (r); return 0;
}
