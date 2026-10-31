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
#include <stdio.h>

static int spans (void *user, size_t offset, size_t n, mulib_readline_style_t style)
{
  size_t *counts = (size_t *)user;
  (void)offset; counts[style] += n; return 0;
}
static int completions (void *user, const char *text, const char *description)
{ size_t *count = (size_t *)user; assert (text && description); ++*count; return 0; }
static int reentry (void *user, const char *text, const char *description)
{
  mulib_readline_t *r = (mulib_readline_t *)user;
  (void)text; (void)description;
  assert (mulib_readline_set (r, "bad", 3) == MULIB_READLINE_BUSY);
  assert (mulib_readline_completion (r, "bad", NULL) == MULIB_READLINE_BUSY);
  return 0;
}
static void key (mulib_readline_t *r, mulib_termkey_code_t code)
{
  mulib_termkey_event_t e;
  memset (&e, 0, sizeof (e)); e.type = MULIB_TERMKEY_KEY; e.key = code;
  assert (!mulib_readline_edit (r, &e));
}
int main (int argc, char **argv)
{
  mulib_readline_t *r = mulib_readline_create (NULL);
  size_t counts[8] = { 0 }, n = 0, start, cursor;
  const char *action = NULL, *args = NULL;
  char *expanded = NULL, *rendered = NULL;
  assert (r);
  assert (!mulib_readline_rule (r, MULIB_READLINE_PREFIX, MULIB_READLINE_COMMENT, "--", NULL, 0));
  assert (!mulib_readline_rule (r, MULIB_READLINE_SPAN, MULIB_READLINE_STRING, "\"", "\"", '\\'));
  assert (!mulib_readline_rule (r, MULIB_READLINE_WORD, MULIB_READLINE_KEYWORD, "local", NULL, 0));
  assert (!mulib_readline_rule (r, MULIB_READLINE_NUMERIC, MULIB_READLINE_NUMBER, NULL, NULL, 0));
  {
    const char *source = "local x=42; \"local\" -- local";
    assert (!mulib_readline_highlight (r, source, strlen (source), spans, counts));
    assert (counts[MULIB_READLINE_KEYWORD] == 5 && counts[MULIB_READLINE_STRING] == 7
        && counts[MULIB_READLINE_NUMBER] == 2 && counts[MULIB_READLINE_COMMENT] == 8);
  }
  assert (!mulib_readline_completion (r, "browser.go", "navigate"));
  assert (!mulib_readline_completion (r, "browser.get", "get"));
  assert (!mulib_readline_set (r, "x=browser.g", 11));
  assert (!mulib_readline_complete (r, completions, &n, &start) && n == 2 && start == 2);
  assert (!mulib_readline_complete (r, reentry, r, NULL));
  assert (!mulib_readline_accept (r, 1));
  assert (!strcmp (mulib_readline_text (r, NULL, NULL), "x=browser.get"));
  assert (!mulib_readline_set (r, "a\xc3\xa9\xe4\xb8\xad", 6));
  key (r, MULIB_TERMKEY_LEFT); key (r, MULIB_TERMKEY_BACKSPACE);
  assert (!strcmp (mulib_readline_text (r, NULL, &cursor), "a\xe4\xb8\xad") && cursor == 1);
  key (r, MULIB_TERMKEY_DELETE);
  assert (!strcmp (mulib_readline_text (r, NULL, NULL), "a"));
  {
    mulib_termkey_event_t paste;
    memset (&paste, 0, sizeof (paste)); paste.type = MULIB_TERMKEY_PASTE_TEXT; paste.length = 1;
    paste.raw[0] = 0xc3; assert (!mulib_readline_edit (r, &paste));
    paste.raw[0] = 0xa9; assert (!mulib_readline_edit (r, &paste));
    paste.raw[0] = '\n'; assert (!mulib_readline_edit (r, &paste));
    assert (!strcmp (mulib_readline_text (r, NULL, NULL), "a\xc3\xa9 "));
  }
  assert (mulib_readline_set (r, "\033[31m", 5) == MULIB_READLINE_INVALID);
  assert (mulib_readline_set (r, "\xc2\x9b", 2) == MULIB_READLINE_INVALID);
  assert (!mulib_readline_history_add (r, "first")); assert (!mulib_readline_history_add (r, "second"));
  assert (!mulib_readline_set (r, "draft", 5)); key (r, MULIB_TERMKEY_UP);
  assert (!strcmp (mulib_readline_text (r, NULL, NULL), "second"));
  key (r, MULIB_TERMKEY_UP); key (r, MULIB_TERMKEY_DOWN); key (r, MULIB_TERMKEY_DOWN);
  assert (!strcmp (mulib_readline_text (r, NULL, NULL), "draft"));
  assert (!mulib_readline_directive (r, "echo", NULL, "print($*)", "print expression"));
  assert (mulib_readline_dispatch (r, ":echo 42", &action, &args, &expanded) == MULIB_READLINE_DIRECTIVE);
  assert (!action && !strcmp (args, "42") && !strcmp (expanded, "print(42)")); free (expanded);
  {
    const char *invalid = "{\"version\":1,\"items\":[{\"text\":\"rolled_back\"},{\"text\":3}]}";
    assert (mulib_readline_load_memory (r, MULIB_READLINE_COMPLETIONS, invalid, strlen (invalid), NULL) == MULIB_READLINE_INVALID);
    assert (!mulib_readline_set (r, "rolled", 6)); n = 0;
    assert (!mulib_readline_complete (r, completions, &n, NULL) && !n);
  }
  assert (!mulib_readline_render (r, "> ", 12, &rendered));
  assert (strstr (rendered, "\033[2K") && strstr (rendered, "rolled")); free (rendered);
  mulib_readline_destroy (r);
  {
    mulib_readline_limits_t limits = { 3, 2, 8, 1, 16 };
    mulib_termkey_event_t e;
    r = mulib_readline_create (&limits); assert (r);
    assert (!mulib_readline_set (r, "a\xc3\xa9", 3));
    memset (&e, 0, sizeof (e)); e.type = MULIB_TERMKEY_KEY; e.key = MULIB_TERMKEY_CHAR; e.codepoint = 'x';
    assert (mulib_readline_edit (r, &e) == MULIB_READLINE_LIMIT);
    assert (!strcmp (mulib_readline_text (r, NULL, NULL), "a\xc3\xa9"));
    assert (!mulib_readline_completion (r, "abc", NULL));
    assert (!mulib_readline_completion (r, "abc", NULL));
    assert (mulib_readline_completion (r, "def", NULL) == MULIB_READLINE_LIMIT);
    assert (!mulib_readline_history_add (r, "one") && !mulib_readline_history_add (r, "two")
        && !mulib_readline_history_add (r, "new"));
    key (r, MULIB_TERMKEY_UP); key (r, MULIB_TERMKEY_UP); key (r, MULIB_TERMKEY_UP);
    assert (!strcmp (mulib_readline_text (r, NULL, NULL), "two"));
    mulib_readline_destroy (r);
  }
  {
    const char *invalid = "{\"version\":1,\"rules\":[{\"kind\":\"number\",\"style\":\"number\",\"escape\":false}]}";
    r = mulib_readline_create (NULL); assert (r);
    assert (mulib_readline_load_memory (r, MULIB_READLINE_SYNTAX, invalid, strlen (invalid), NULL) == MULIB_READLINE_INVALID);
    assert (!mulib_readline_render (r, "", 8, &rendered));
    assert (!strstr (rendered, "[0C")); free (rendered);
    mulib_readline_destroy (r);
  }
  if (argc > 1)
    {
      static const char *const languages[] = { "c", "cpp", "python", "javascript", "typescript", "lua", "rust", "go", "java", "csharp", "ruby", "php", "bash", "sql", "json" };
      static const char *const extensions[] = { "mlsyn", "mlac", "mldir" };
      for (size_t i = 0; i < 15; ++i)
        {
          r = mulib_readline_create (NULL); assert (r);
          for (size_t j = 0; j < 3; ++j)
            { char path[4096]; (void)snprintf (path, sizeof (path), "%s/%s.%s", argv[1], languages[i], extensions[j]); assert (!mulib_readline_load (r, path, NULL)); }
          mulib_readline_destroy (r);
        }
    }
  puts ("readline: editing, highlighting, completions, transactional profiles and directives passed");
  return 0;
}
