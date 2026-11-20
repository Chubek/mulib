#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_TERMSCRIPT_AST
#include "mulib-termscript-ast.h"

#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const mulib_termscript_node_t *
node (const mulib_termscript_program_t *p, size_t index,
      mulib_termscript_kind_t kind, const char *text)
{
  assert (index < p->count);
  const mulib_termscript_node_t *n = &p->nodes[index];
  assert (n->kind == kind && n->offset <= p->length);
  assert (n->length == strlen (text) && n->length <= p->length - n->offset);
  assert (!memcmp (p->source + n->offset, text, n->length));
  return n;
}

static const mulib_termscript_node_t *
leaf (const mulib_termscript_program_t *p, size_t index,
      mulib_termscript_kind_t kind, const char *text)
{
  const mulib_termscript_node_t *n = node (p, index, kind, text);
  assert (n->first == SIZE_MAX);
  return n;
}

static void
test_structure (void)
{
  char source[] = "% ignored\t\r\n"
                  "%module: sample.demo\r\n"
                  "{ ( -9223372036854775808 +9223372036854775807, \"a\\n\\x1b\" ) "
                  "[ #scalar @arr %rec $1 !Ctrl-D, #\\z #\\\xf0\x9f\x99\x82 ] "
                  "< k: 2, b: \"fff\", inner:<j:[{}]>> `read \\`word\\`` }\n"
                  "%if-term: xterm*\n"
                  "  #\\space\n"
                  "%else:\n"
                  "  #\\}\n"
                  "%end:";
  const char *block_text = "{ ( -9223372036854775808 +9223372036854775807, \"a\\n\\x1b\" ) "
                           "[ #scalar @arr %rec $1 !Ctrl-D, #\\z #\\\xf0\x9f\x99\x82 ] "
                           "< k: 2, b: \"fff\", inner:<j:[{}]>> `read \\`word\\`` }";
  mulib_termscript_program_t p = { 0 };
  mulib_termscript_error_t e = { 42, "old error" };
  const mulib_termscript_node_t *n, *block, *tuple, *array, *record, *field;
  assert (mulib_termscript_parse (source, sizeof source - 1, &p, &e));
  assert (!e.message && e.offset == 0);
  assert (p.source != source && p.length == sizeof source - 1);
  assert (!memcmp (source, p.source, sizeof source));
  memset (source, 'x', sizeof source);

  n = leaf (&p, p.root, MULIB_TERMSCRIPT_DIRECTIVE, "%module: sample.demo");
  assert (n->offset == strlen ("% ignored\t\r\n"));
  block = node (&p, n->next, MULIB_TERMSCRIPT_BLOCK, block_text);
  tuple = node (&p, block->first, MULIB_TERMSCRIPT_TUPLE,
                "( -9223372036854775808 +9223372036854775807, \"a\\n\\x1b\" )");
  n = leaf (&p, tuple->first, MULIB_TERMSCRIPT_INTEGER, "-9223372036854775808");
  assert (n->integer == INT64_MIN);
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_INTEGER, "+9223372036854775807");
  assert (n->integer == INT64_MAX);
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_STRING, "a\\n\\x1b");
  assert (n->next == SIZE_MAX && p.source[n->offset - 1] == '"');

  array = node (&p, tuple->next, MULIB_TERMSCRIPT_ARRAY,
                "[ #scalar @arr %rec $1 !Ctrl-D, #\\z #\\\xf0\x9f\x99\x82 ]");
  n = leaf (&p, array->first, MULIB_TERMSCRIPT_SYMBOL, "scalar");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_ARRAY_SYMBOL, "arr");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_RECORD_SYMBOL, "rec");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_PARAMETER, "1");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_KEY, "Ctrl-D");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_CHARACTER, "#\\z");
  assert (n->integer == 'z');
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_CHARACTER, "#\\\xf0\x9f\x99\x82");
  assert (n->integer == 0x1f642 && n->next == SIZE_MAX);

  record = node (&p, array->next, MULIB_TERMSCRIPT_RECORD,
                 "< k: 2, b: \"fff\", inner:<j:[{}]>>");
  field = node (&p, record->first, MULIB_TERMSCRIPT_FIELD, "k");
  n = leaf (&p, field->first, MULIB_TERMSCRIPT_INTEGER, "2");
  assert (n->integer == 2 && n->next == SIZE_MAX);
  field = node (&p, field->next, MULIB_TERMSCRIPT_FIELD, "b");
  assert (leaf (&p, field->first, MULIB_TERMSCRIPT_STRING, "fff")->next
          == SIZE_MAX);
  field = node (&p, field->next, MULIB_TERMSCRIPT_FIELD, "inner");
  assert (field->next == SIZE_MAX);
  n = node (&p, field->first, MULIB_TERMSCRIPT_RECORD, "<j:[{}]>");
  assert (n->next == SIZE_MAX);
  n = node (&p, n->first, MULIB_TERMSCRIPT_FIELD, "j");
  n = node (&p, n->first, MULIB_TERMSCRIPT_ARRAY, "[{}]");
  n = leaf (&p, n->first, MULIB_TERMSCRIPT_BLOCK, "{}");
  assert (n->next == SIZE_MAX);
  n = leaf (&p, record->next, MULIB_TERMSCRIPT_COMMAND, "read \\`word\\`");
  assert (n->next == SIZE_MAX && p.source[n->offset - 1] == '`');

  n = leaf (&p, block->next, MULIB_TERMSCRIPT_DIRECTIVE, "%if-term: xterm*");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_CHARACTER, "#\\space");
  assert (n->integer == ' ');
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_DIRECTIVE, "%else:");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_CHARACTER, "#\\}");
  assert (n->integer == '}');
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_DIRECTIVE, "%end:");
  assert (n->next == SIZE_MAX && p.count == 30);
  mulib_termscript_program_free (&p);
  mulib_termscript_program_free (&p);
  mulib_termscript_program_free (NULL);
  assert (!p.source && !p.nodes && !p.count);
}

static void
test_empty_and_bare (void)
{
  const char comments[] = " \t% comment\n   % \r";
  const char source[] = { '-', ';', '2', 'a', ';', 'a', ':', ':', 'b',
                         ' ', '"', '"', ';', '`', '`', ' ', '<', '>',
                         ' ', '(', ')', ' ', '[', ']' };
  mulib_termscript_program_t p = { 0 };
  const mulib_termscript_node_t *n;
  assert (mulib_termscript_parse (NULL, 0, &p, NULL));
  assert (p.root == SIZE_MAX && !p.nodes && !p.count && !p.length);
  mulib_termscript_program_free (&p);
  assert (mulib_termscript_parse (comments, sizeof comments - 1, &p, NULL));
  assert (p.root == SIZE_MAX && !p.nodes && !p.count);
  mulib_termscript_program_free (&p);
  assert (mulib_termscript_parse (source, sizeof source, &p, NULL));
  n = leaf (&p, p.root, MULIB_TERMSCRIPT_WORD, "-");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_WORD, "2a");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_WORD, "a::b");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_STRING, "");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_COMMAND, "");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_RECORD, "<>");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_TUPLE, "()");
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_ARRAY, "[]");
  assert (n->next == SIZE_MAX);
  mulib_termscript_program_free (&p);
}

static void
test_escape_literals (void)
{
  enum { limit = 63 };
  const char source[] = "[[CUP]] [[ \t\r\npaste-on\v ]] [[kf11]] "
                        "[ [0,[1]] [2], <cap:[[unknown_123]]> ] "
                        "\"[[ordinary text]]\"";
  char longest[limit + 4];
  mulib_termscript_program_t p = { 0 };
  const mulib_termscript_node_t *n, *outer, *inner, *nested, *record, *field;
  assert (mulib_termscript_parse (source, sizeof source - 1, &p, NULL));
  n = leaf (&p, p.root, MULIB_TERMSCRIPT_ESCAPE, "CUP");
  assert (n->offset == 2 && n->integer == 0);
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_ESCAPE, "paste-on");
  assert (n->offset == (size_t)(strstr (source, "paste-on") - source));
  n = leaf (&p, n->next, MULIB_TERMSCRIPT_ESCAPE, "kf11");
  outer = node (&p, n->next, MULIB_TERMSCRIPT_ARRAY,
                "[ [0,[1]] [2], <cap:[[unknown_123]]> ]");
  inner = node (&p, outer->first, MULIB_TERMSCRIPT_ARRAY, "[0,[1]]");
  n = leaf (&p, inner->first, MULIB_TERMSCRIPT_INTEGER, "0");
  nested = node (&p, n->next, MULIB_TERMSCRIPT_ARRAY, "[1]");
  n = leaf (&p, nested->first, MULIB_TERMSCRIPT_INTEGER, "1");
  assert (nested->next == SIZE_MAX && n->next == SIZE_MAX);
  nested = node (&p, inner->next, MULIB_TERMSCRIPT_ARRAY, "[2]");
  n = leaf (&p, nested->first, MULIB_TERMSCRIPT_INTEGER, "2");
  assert (n->next == SIZE_MAX);
  record = node (&p, nested->next, MULIB_TERMSCRIPT_RECORD, "<cap:[[unknown_123]]>");
  field = node (&p, record->first, MULIB_TERMSCRIPT_FIELD, "cap");
  n = leaf (&p, field->first, MULIB_TERMSCRIPT_ESCAPE, "unknown_123");
  assert (record->next == SIZE_MAX && field->next == SIZE_MAX && n->next == SIZE_MAX);
  n = leaf (&p, outer->next, MULIB_TERMSCRIPT_STRING, "[[ordinary text]]");
  assert (n->next == SIZE_MAX);
  mulib_termscript_program_free (&p);

  longest[0] = longest[1] = '[';
  memset (longest + 2, '_', limit);
  longest[limit + 2] = longest[limit + 3] = ']';
  assert (mulib_termscript_parse (longest, sizeof longest, &p, NULL));
  assert (p.count == 1 && p.nodes[p.root].length == limit);
  mulib_termscript_program_free (&p);
}

static void
reject (const char *source, size_t length, size_t offset, const char *message)
{
  mulib_termscript_program_t p = { 0 }, saved;
  mulib_termscript_error_t e = { 0, NULL };
  assert (mulib_termscript_parse ("10", 2, &p, NULL));
  saved = p;
  assert (!mulib_termscript_parse (source, length, &p, &e));
  assert (e.offset == offset && e.message && strstr (e.message, message));
  assert (p.source == saved.source && p.nodes == saved.nodes
          && p.length == saved.length && p.count == saved.count
          && p.root == saved.root);
  assert (p.nodes[p.root].integer == 10 && !strcmp (p.source, "10"));
  mulib_termscript_program_free (&p);
}

#define REJECT(text, offset, message) reject (text, sizeof (text) - 1, offset, message)

static void
test_errors (void)
{
  char long_escape[68];
  REJECT ("word [1", 7, "unclosed");
  REJECT ("{ ( ] }", 4, "closing");
  REJECT ("<a 2>", 3, "':'");
  REJECT ("<a: > ", 4, "closing");
  REJECT ("<a: 1 b: 2>", 6, "','");
  REJECT ("<: 2>", 1, "field name");
  REJECT ("[1,,2]", 3, "unexpected");
  REJECT ("@", 1, "sigil");
  REJECT ("%\n", 1, "sigil");
  REJECT ("#\\", 2, "expected character");
  REJECT ("#\\letters", 2, "one Unicode character");
  REJECT ("\"foo\\", 5, "unterminated");
  REJECT ("`foo\\`", 6, "unterminated");
  REJECT ("\"foo\\q\"", 4, "unknown quoted escape");
  REJECT ("`\\x0`", 1, "hex digits");
  REJECT ("\"\\xg0\"", 1, "hex digits");
  REJECT (" 9223372036854775808", 1, "out of range");
  REJECT ("-9223372036854775809", 0, "out of range");
  REJECT ("abc\0def", 3, "NUL");
  REJECT ("\"a\xf0\x9f\"", 2, "UTF-8");
  REJECT ("% bad utf8 \xed\xa0\x80", 11, "UTF-8");
  REJECT ("\xc0\x80", 0, "UTF-8");
  REJECT ("[[[[", 2, "escape name");
  REJECT ("[[]]", 2, "escape name");
  REJECT ("[[ \t ]]", 5, "escape name");
  REJECT ("[[\xc3\xa9]]", 2, "escape name");
  REJECT ("[[foo.bar]]", 5, "']]'");
  REJECT ("[[a b]]", 4, "']]'");
  REJECT ("[[CUP] ]", 6, "']]'");
  REJECT ("[[CUP]", 6, "']]'");
  long_escape[0] = long_escape[1] = '[';
  memset (long_escape + 2, 'A', 64);
  long_escape[66] = long_escape[67] = ']';
  reject (long_escape, sizeof long_escape, 65, "escape name limit");
  reject (NULL, 1, 0, "argument");
  reject ("x", SIZE_MAX, 1024 * 1024, "source limit");
  assert (!mulib_termscript_parse ("", 0, NULL, NULL));
}

static void
test_limits (void)
{
  mulib_termscript_program_t p = { 0 };
  const size_t limit = 1024 * 1024, nodes = 65536, depth = 128;
  char *source = (char *)malloc (limit);
  assert (source);
  /* Worst-case scanner path: an entire source of escaped quotes. */
  source[0] = source[limit - 1] = '"';
  for (size_t i = 1; i < limit - 1; i += 2)
    {
      source[i] = '\\';
      source[i + 1] = '"';
    }
  assert (mulib_termscript_parse (source, limit, &p, NULL));
  assert (p.count == 1 && p.nodes[p.root].length == limit - 2);
  mulib_termscript_program_free (&p);

  memset (source, '{', depth + 1);
  memset (source + depth + 1, '}', depth + 1);
  assert (mulib_termscript_parse (source + 1, depth * 2, &p, NULL));
  assert (p.count == depth);
  mulib_termscript_program_free (&p);
  reject (source, (depth + 1) * 2, depth, "nesting limit");

  for (size_t i = 0; i < nodes + 1; ++i)
    {
      source[2 * i] = 'x';
      source[2 * i + 1] = ' ';
    }
  assert (mulib_termscript_parse (source, nodes * 2, &p, NULL));
  assert (p.count == nodes);
  mulib_termscript_program_free (&p);
  reject (source, (nodes + 1) * 2, nodes * 2, "node limit");
  free (source);
}

int
main (void)
{
  test_structure ();
  test_empty_and_bare ();
  test_escape_literals ();
  test_errors ();
  test_limits ();
  return 0;
}
