#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Inject failures into both the compiler's nodes and its buffer dependency. */
static size_t allocation_count, fail_at;
static void *
webidl_calloc (size_t n, size_t size)
{
  if (++allocation_count == fail_at)
    return NULL;
  return calloc (n, size);
}
static void *
webidl_realloc (void *p, size_t size)
{
  if (++allocation_count == fail_at)
    return NULL;
  return realloc (p, size);
}
#define calloc webidl_calloc
#define realloc webidl_realloc
#define MULIB_IMPL_WEBIDL
#define MULIB_IMPL_DBSCHEMA
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-webidl.h"
#undef calloc
#undef realloc

static const char fixture[]
    = "// source comment\n"
      "[Exposed=(Window,Worker)] interface Sample : Base {\n"
      "  constructor(optional DOMString name = \"\");\n"
      "  readonly attribute unsigned long count;\n"
      "  attribute DOMString label;\n"
      "  static long sum(long a, optional long b = 0);\n"
      "  undefined send(ByteString... items);\n"
      "  Promise<undefined> ready();\n"
      "  const double PI = 3.14;\n"
      "  const unsigned long long MAX = 18446744073709551615;\n"
      "  const long long MIN = -9223372036854775808;\n"
      "  const unrestricted double INF = - /* trivia */ Infinity;\n"
      "};\n"
      "interface Base {};\n"
      "dictionary Options : Parent {\n"
      "  required long id;\n"
      "  sequence<DOMString> names = [];\n"
      "  (DOMString or long)? choice = null;\n"
      "  record<USVString, sequence<long>> values;\n"
      "  Handler handler;\n"
      "  Count count;\n"
      "};\n"
      "dictionary Parent {};\n"
      "enum Mode {\"fast\", \"slow\", \"caf\xc3\xa9\", \"back\\slash?"
      "?/\",};\n"
      "typedef unsigned /* canonical trivia */ long long Count;\n"
      "callback Handler = boolean (Options data, optional Mode mode = "
      "\"fast\");\n"
      "namespace Utility { readonly attribute long version; undefined run(); "
      "};\n"
      "typedef sequence<long> Counts; interface List { attribute Counts "
      "counts; };\n"
      "partial interface List { undefined clear(); };\n"
      "interface mixin Labelled { readonly attribute DOMString label; };\n"
      "partial interface mixin Labelled { undefined reset(); };\n"
      "List includes Labelled;\n"
      "partial dictionary Parent { long generation = 0; };\n"
      "typedef undefined Nothing; typedef (undefined or long) MaybeCount;\n"
      "interface Async { Nothing finish(); MaybeCount count(); "
      "undefined consume(async_sequence<DOMString> values); };\n"
      "typedef unsigned long long _boolean; interface _long {};"
      "dictionary Escaped { _boolean count; _long _object; };\n";

static int
contains (const mulib_buffer_t *b, const char *s)
{
  size_t i, n = strlen (s);
  for (i = 0; n <= b->length && i <= b->length - n; ++i)
    if (!memcmp (b->data + i, s, n))
      return 1;
  return 0;
}
static void
reject (const char *source, mulib_webidl_format_t format)
{
  mulib_buffer_t b = { 0 };
  mulib_webidl_options_t o = { MULIB_WEBIDL_SEXPR, MULIB_WEBIDL_AST };
  mulib_webidl_error_t e;
  o.format = format;
  assert (mulib_buffer_append (&b, "prefix", 6));
  {
    int ok = mulib_webidl_compile (source, strlen (source), &o, &b, &e);
    if (ok)
      fprintf (stderr, "unexpectedly accepted: %s\n", source);
    assert (!ok);
  }
  assert (e.message && e.offset <= strlen (source) && e.line && e.column);
  assert (b.length == 6 && !memcmp (b.data, "prefix", 6));
  mulib_buffer_free (&b);
}
int
main (int argc, char **argv)
{
  mulib_webidl_options_t o = { MULIB_WEBIDL_SEXPR, MULIB_WEBIDL_AST };
  mulib_webidl_error_t e;
  mulib_buffer_t b = { 0 };
  size_t i, total;
  if (argc == 3 && !strcmp (argv[1], "--check-file"))
    {
      mulib_buffer_t input = { 0 };
      unsigned char bytes[4096];
      size_t n;
      int ok;
      FILE *file = fopen (argv[2], "rb");
      assert (file);
      while ((n = fread (bytes, 1, sizeof bytes, file)) != 0)
        assert (mulib_buffer_append (&input, bytes, n));
      assert (!ferror (file));
      assert (!fclose (file));
      ok = mulib_webidl_compile ((const char *)input.data, input.length, NULL,
                                 &b, &e);
      if (!ok)
        fprintf (stderr, "%s:%zu:%zu: %s\n", argv[2], e.line, e.column,
                 e.message);
      mulib_buffer_free (&input);
      mulib_buffer_free (&b);
      return !ok;
    }
  if (argc == 2)
    {
      if (!strcmp (argv[1], "--source"))
        {
          assert (fwrite (fixture, 1, sizeof fixture - 1, stdout)
                  == sizeof fixture - 1);
          return 0;
        }
      if (!strcmp (argv[1], "--emit"))
        o.format = MULIB_WEBIDL_C99;
      else if (!strcmp (argv[1], "--emit-yaml"))
        o.format = MULIB_WEBIDL_CODEYAML;
      else if (!strcmp (argv[1], "--emit-cst"))
        o.tree = MULIB_WEBIDL_CST;
      else
        assert (!strcmp (argv[1], "--emit-ast"));
      assert (mulib_webidl_compile (fixture, sizeof fixture - 1, &o, &b, &e));
      assert (fwrite (b.data, 1, b.length, stdout) == b.length);
      mulib_buffer_free (&b);
      return 0;
    }
  assert (mulib_webidl_compile (fixture, sizeof fixture - 1, NULL, &b, &e));
  assert (!e.message && !e.offset && !e.line && !e.column);
  assert (contains (&b, "(interface")
          && contains (&b, "(type \"unsigned long long\")"));
  assert (contains (&b, "(nullable)") && !contains (&b, "source comment"));
  b.length = 0;
  o.tree = MULIB_WEBIDL_CST;
  assert (mulib_webidl_compile (fixture, sizeof fixture - 1, &o, &b, &e));
  assert (contains (&b, "(webidl-cst")
          && contains (&b, "(comment 0 \"// source comment\")"));
  assert (contains (&b, "(whitespace") && contains (&b, "(punctuation"));
  b.length = 0;
  o.tree = MULIB_WEBIDL_AST;
  o.format = MULIB_WEBIDL_CODEYAML;
  assert (mulib_webidl_compile (fixture, sizeof fixture - 1, &o, &b, &e));
  assert (contains (&b, "kind: webidl\nchildren:")
          && contains (&b, "kind: argument"));
  b.length = 0;
  o.format = MULIB_WEBIDL_C99;
  assert (mulib_webidl_compile (fixture, sizeof fixture - 1, &o, &b, &e));
  assert (contains (&b, "typedef struct webidl_Sample webidl_Sample;"));
  assert (contains (&b, "webidl_Sample__webidl_count__get"));
  assert (contains (&b, "has__webidl_names") && contains (&b, "int is_null;"));
  assert (contains (&b, "webidl__has_1") && contains (&b, "webidl__count_0"));
  assert (contains (&b, "webidl_Utility__webidl_run__call_1(void)"));
  mulib_buffer_free (&b);
  {
    const mulib_dbschema_field_t fields[]
        = { { "name", MULIB_JSON_STRING, 1, 0 },
            { "values", MULIB_JSON_ARRAY, 0, 1 } };
    const mulib_dbschema_t schema = { "Person", fields, 2 };
    assert (mulib_webidl_generate (&schema, &b));
    assert (
        contains (&b, "dictionary _Person {\n  required DOMString _name;\n"));
    assert (contains (&b, "sequence<any>? _values;"));
    assert (
        mulib_webidl_compile ((const char *)b.data, b.length, NULL, &b, &e));
    mulib_buffer_free (&b);
    {
      mulib_dbschema_t invalid = schema;
      const mulib_dbschema_field_t reserved[] = {
        { "constructor", MULIB_JSON_STRING, 0, 0 }
      };
      invalid.name = "_Private";
      assert (mulib_buffer_append (&b, "prefix", 6));
      assert (!mulib_webidl_generate (&invalid, &b));
      invalid.name = "Valid";
      invalid.fields = reserved;
      invalid.count = 1;
      assert (!mulib_webidl_generate (&invalid, &b));
      assert (b.length == 6 && !memcmp (b.data, "prefix", 6));
      mulib_buffer_free (&b);
    }
  }

  /* Empty source is a valid definitions list in all output languages. */
  for (i = 0; i < 3; ++i)
    {
      o.format = (mulib_webidl_format_t)i;
      assert (mulib_webidl_compile (NULL, 0, &o, &b, NULL));
      assert (b.length);
      b.length = 0;
    }
  assert (!mulib_webidl_compile (NULL, 1, &o, &b, &e));
  assert (!mulib_webidl_compile ("", 16777217, &o, &b, &e));
  assert (!mulib_webidl_compile ("", 0, &o, NULL, &e));
  o.tree = MULIB_WEBIDL_CST;
  assert (!mulib_webidl_compile ("", 0, &o, &b, &e));
  o.tree = MULIB_WEBIDL_AST;
  o.format = (mulib_webidl_format_t)99;
  assert (!mulib_webidl_compile ("", 0, &o, &b, &e));
  assert (!mulib_webidl_compile ("\0", 1, NULL, &b, &e));
  assert (!mulib_webidl_compile ("//\xff", 3, NULL, &b, &e));
  assert (!mulib_webidl_compile ("\r\ninterface X {\n", 16, NULL, &b, &e));
  assert (e.line == 3 && e.column == 1);
  mulib_buffer_free (&b);

  reject ("interface X {", MULIB_WEBIDL_SEXPR);
  reject ("dictionary X { required long x = 2; };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { void x(long a = 1); };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { void x(optional long... a); };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { void x(long... a, long b); };", MULIB_WEBIDL_SEXPR);
  reject ("typedef sequence<long X;", MULIB_WEBIDL_SEXPR);
  reject ("typedef any? X;", MULIB_WEBIDL_SEXPR);
  reject ("typedef void X;", MULIB_WEBIDL_SEXPR);
  reject ("typedef record<long, long> X;", MULIB_WEBIDL_SEXPR);
  reject ("typedef record<DOMString?, long> X;", MULIB_WEBIDL_SEXPR);
  reject ("typedef (any or long) X;", MULIB_WEBIDL_SEXPR);
  reject ("typedef (Promise<long> or long) X;", MULIB_WEBIDL_SEXPR);
  reject ("typedef (DOMString? or long)? X;", MULIB_WEBIDL_SEXPR);
  reject ("dictionary X { (undefined or long) value; };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { undefined f(undefined value); };",
          MULIB_WEBIDL_SEXPR);
  reject ("enum X {};", MULIB_WEBIDL_SEXPR);
  reject ("enum X {\"unterminated};", MULIB_WEBIDL_SEXPR);
  reject ("/* unterminated", MULIB_WEBIDL_SEXPR);
  reject ("[Foo=(])] interface X {};", MULIB_WEBIDL_SEXPR);
  reject ("interface X { const long X = 0x; };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { const long X = 09; };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { const double X = 1e+; };", MULIB_WEBIDL_SEXPR);
  reject ("typedef Unknown X;", MULIB_WEBIDL_C99);
  reject ("typedef Y X; typedef X Y;", MULIB_WEBIDL_C99);
  reject ("interface X { iterable<long>; };", MULIB_WEBIDL_C99);
  reject ("partial interface X {};", MULIB_WEBIDL_C99);
  reject ("dictionary X { long a; long a; };", MULIB_WEBIDL_SEXPR);
  reject ("enum X {\"a\", \"a\"};", MULIB_WEBIDL_SEXPR);
  reject ("interface X {}; interface X {};", MULIB_WEBIDL_SEXPR);
  reject ("interface X { static; };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { static getter long x(); };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { readonly iterable<long>; };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { iterable<long>(); };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { getter attribute long x; };", MULIB_WEBIDL_SEXPR);
  reject ("interface mixin X { static long x(); };", MULIB_WEBIDL_SEXPR);
  reject ("callback interface X { attribute long x; };", MULIB_WEBIDL_SEXPR);
  reject ("callback interface X : Y {};", MULIB_WEBIDL_SEXPR);
  reject ("namespace X { attribute long x; };", MULIB_WEBIDL_SEXPR);
  reject ("partial interface X { constructor(); };", MULIB_WEBIDL_SEXPR);
  reject ("interface X { const long? x = 0; };", MULIB_WEBIDL_SEXPR);
  reject ("interface interface {};", MULIB_WEBIDL_SEXPR);
  reject ("interface _ {};", MULIB_WEBIDL_SEXPR);
  reject ("interface _1 {};", MULIB_WEBIDL_SEXPR);
  reject ("interface __X {};", MULIB_WEBIDL_SEXPR);
  reject ("interface X { attribute long constructor; };", MULIB_WEBIDL_SEXPR);
  reject ("interface X {}; partial interface X { attribute long a; };"
          "partial interface X { attribute long a; };",
          MULIB_WEBIDL_C99);
  reject ("dictionary X {}; interface mixin M {}; X includes M;",
          MULIB_WEBIDL_C99);
  reject ("interface X {}; X includes Missing;", MULIB_WEBIDL_C99);
  reject ("namespace X { constructor(); };", MULIB_WEBIDL_SEXPR);
  reject ("interface X : Missing {};", MULIB_WEBIDL_C99);
  reject ("interface X : Y {}; interface Y : X {};", MULIB_WEBIDL_C99);
  reject ("interface X { Promise<Missing> x(); };", MULIB_WEBIDL_C99);
  reject ("interface X { const DOMString X = \"x\"; };", MULIB_WEBIDL_C99);
  reject (
      "interface X { const unsigned long long X = 18446744073709551616; };",
      MULIB_WEBIDL_C99);

  /* Rich syntax is preserved in the semantic formats even without C lowering.
   */
  {
    const char *extra
        = "interface mixin M { stringifier; }; X includes M;"
          "partial interface X { getter DOMString (unsigned long i); };"
          "interface Y { readonly maplike<DOMString, long>; setlike<long>;"
          "async iterable<long>(optional long start = 0); };"
          "callback interface Listener { undefined handle(); };"
          "interface -Vendor { async_iterable<DOMString, long>; };"
          "enum Multiline {\"first\nsecond\"};"
          "[Conditional=A&B|C, Custom=foo.bar] interface _interface {};"
          "typedef undefined? NullableUndefined;"
          "dictionary Defaults { any value = undefined; };"
          "interface Ordered { constructor(optional DOMString message = \"\", "
          "object options); };"
          "interface Names { attribute long required; undefined includes(long "
          "interface); };";
    assert (mulib_webidl_compile (extra, strlen (extra), NULL, &b, &e));
    mulib_buffer_free (&b);
  }
  /* Bounded recursion and byte-span ownership (including aliased output). */
  {
    const char *trivia = "typedef long     Count; typedef unrestricted float "
                         "/* trailing */ Real;";
    assert (mulib_webidl_compile (trivia, strlen (trivia), NULL, &b, &e));
    assert (contains (&b, "(type \"long\")")
            && contains (&b, "(type \"unrestricted float\")"));
    mulib_buffer_free (&b);
  }
  {
    char deep[2048];
    size_t n = 0;
    memcpy (deep, "typedef ", 8);
    n = 8;
    for (i = 0; i < 70; ++i)
      {
        memcpy (deep + n, "sequence<", 9);
        n += 9;
      }
    memcpy (deep + n, "long", 4);
    n += 4;
    for (i = 0; i < 70; ++i)
      deep[n++] = '>';
    memcpy (deep + n, " X;", 3);
    n += 3;
    assert (!mulib_webidl_compile (deep, n, NULL, &b, &e));
    assert (strstr (e.message, "nesting"));
    assert (mulib_buffer_append (&b, "interface X {};", 15));
    assert (
        mulib_webidl_compile ((const char *)b.data, b.length, NULL, &b, &e));
    assert (!memcmp (b.data, "interface X {};", 15));
    mulib_buffer_free (&b);
  }
  /* Token/node limits are independent of the nesting limit. */
  {
    mulib_buffer_t input = { 0 };
    for (i = 0; i < 131073; ++i)
      assert (mulib_buffer_append (&input, "/**/ ", 5));
    assert (!mulib_webidl_compile ((const char *)input.data, input.length,
                                   NULL, &b, &e));
    assert (strstr (e.message, "token limit"));
    input.length = 0;
    assert (mulib_buffer_append (&input, "dictionary X {", 14));
    for (i = 0; i < 22000; ++i)
      assert (mulib_buffer_append (&input, "long x;", 7));
    assert (mulib_buffer_append (&input, "};", 2));
    assert (!mulib_webidl_compile ((const char *)input.data, input.length,
                                   NULL, &b, &e));
    assert (strstr (e.message, "node limit"));
    mulib_buffer_free (&input);
  }
  /* Truncations and bounded deterministic mutations exercise cleanup paths. */
  for (i = 0; i < sizeof fixture - 1; ++i)
    {
      int ok = mulib_webidl_compile (fixture, i, NULL, &b, &e);
      assert (ok || (e.message && e.offset <= i));
      b.length = 0;
    }
  {
    uint32_t state = 0x12345678;
    char bytes[96];
    for (i = 0; i < 3000; ++i)
      {
        size_t j;
        int ok;
        for (j = 0; j < sizeof bytes; ++j)
          {
            state = state * 1664525u + 1013904223u;
            bytes[j] = (char)(state >> 24);
          }
        ok = mulib_webidl_compile (bytes, i % sizeof bytes, NULL, &b, &e);
        assert (ok || e.message);
        b.length = 0;
      }
  }
  mulib_buffer_free (&b);
  /* Every allocation site is fallible, with atomic output and a diagnostic. */
  for (i = 0; i < 3; ++i)
    {
      size_t j;
      o.format = (mulib_webidl_format_t)i;
      allocation_count = 0;
      assert (mulib_webidl_compile (fixture, sizeof fixture - 1, &o, &b, &e));
      total = allocation_count;
      mulib_buffer_free (&b);
      for (j = 1; j <= total; ++j)
        {
          allocation_count = 0;
          fail_at = j;
          assert (
              !mulib_webidl_compile (fixture, sizeof fixture - 1, &o, &b, &e));
          assert (e.message && b.length == 0);
          mulib_buffer_free (&b);
        }
      fail_at = 0;
    }
  return 0;
}
