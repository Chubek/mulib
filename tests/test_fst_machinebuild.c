#include "mulib-machinebuild.h"
#include <assert.h>

static size_t allocation_count, fail_after = SIZE_MAX;
static void *test_malloc (size_t n)
{ return allocation_count++ == fail_after ? NULL : malloc (n); }
static void *test_calloc (size_t n, size_t stride)
{ return allocation_count++ == fail_after ? NULL : calloc (n, stride); }
static void *test_realloc (void *p, size_t n)
{ return allocation_count++ == fail_after ? NULL : realloc (p, n); }
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_VEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_FST
#define MULIB_IMPL_MACHINEBUILD
#include "mulib-vec.h"
#include "mulib-strbuf.h"
#include "mulib-fst.h"
#include "mulib-machinebuild.h"
#undef malloc
#undef calloc
#undef realloc

static const char source[] =
  "# Forward references, constant emissions and silent transitions\n"
  "machine demo {\n"
  "  start idle;\n"
  "  transition idle -> active on 97..122 emit 65;\n"
  "  transition active -> active on 0..9; // no output\n"
  "  transition active -> done on 4294967295 emit 0;\n"
  "  transition done -> idle on 42 emit 4294967295;\n"
  "  state idle accept; state active; state done accept;\n"
  "}\n";

static void test_fst (void)
{
  mulib_fst_t *f = mulib_fst_create (2, 3);
  mulib_fst_result_t result = {0};
  mulib_fst_state_t state;
  mulib_fst_edge_t edge;
  const uint32_t input[] = {0, UINT32_MAX, 0};
  size_t a, b, id = 99, next = 99, work = 99;
  uint32_t output = 99;
  int emits = 99;
  assert (f);
  assert (mulib_fst_add_state (f, 1, &a) == MULIB_FST_OK && a == 0);
  assert (mulib_fst_add_state (f, 0, &b) == MULIB_FST_OK && b == 1);
  assert (mulib_fst_add_state (f, 0, &id) == MULIB_FST_LIMIT && id == 99);
  assert (mulib_fst_add_state (f, 0, NULL) == MULIB_FST_INVALID);
  assert (mulib_fst_add_range (f, a, b, 0, 0, 1, UINT32_MAX, &id) == MULIB_FST_OK && id == 0);
  assert (mulib_fst_add_range (f, a, a, 1, UINT32_MAX, 0, 0, NULL) == MULIB_FST_OK);
  assert (mulib_fst_add_range (f, b, a, UINT32_MAX, UINT32_MAX, 1, 0, NULL) == MULIB_FST_OK);
  id = 99;
  assert (mulib_fst_add_range (f, a, a, 0, 1, 0, 0, &id) == MULIB_FST_CONFLICT && id == 99);
  assert (mulib_fst_add_range (f, b, a, 0, 1, 0, 0, NULL) == MULIB_FST_LIMIT);
  assert (mulib_fst_add_range (f, 2, a, 0, 1, 0, 0, NULL) == MULIB_FST_INVALID);
  assert (mulib_fst_add_range (f, b, a, 2, 1, 0, 0, NULL) == MULIB_FST_INVALID);
  assert (mulib_fst_state_count (f) == 2 && mulib_fst_edge_count (f) == 3);
  assert (!mulib_fst_state_count (NULL) && !mulib_fst_edge_count (NULL));
  assert (mulib_fst_state (f, a, &state) == MULIB_FST_OK && state.accepting);
  assert (mulib_fst_edge (f, state.first_edge, &edge) == MULIB_FST_OK && edge.lower == 1);
  assert (mulib_fst_state (f, 2, &state) == MULIB_FST_INVALID);
  assert (mulib_fst_edge (f, 3, &edge) == MULIB_FST_INVALID);
  assert (mulib_fst_step (f, a, 0, 1, &next, &emits, &output, &work) == MULIB_FST_LIMIT);
  assert (work == 1 && next == 99 && emits == 99 && output == 99);
  assert (mulib_fst_step (f, b, 0, 0, &next, &emits, &output, &work) == MULIB_FST_NOMATCH);
  assert (work == 2 && next == 99 && emits == 99 && output == 99);
  assert (mulib_fst_step (f, a, 0, 3, &next, &emits, &output, &work) == MULIB_FST_OK);
  assert (next == b && emits && output == UINT32_MAX && work == 3);
  assert (mulib_fst_step (f, 2, 0, 0, &next, &emits, &output, &work) == MULIB_FST_INVALID && work == 0);
  assert (mulib_fst_transduce (f, a, NULL, 0, 1, &result, &work) == MULIB_FST_OK);
  assert (!result.length && !result.output && result.final_state == a && work == 0);
  assert (mulib_fst_transduce (f, a, input, 2, 5, &result, &work) == MULIB_FST_OK);
  assert (work == 5 && result.length == 2 && result.output[0] == UINT32_MAX && result.output[1] == 0);
  {
    uint32_t *old = result.output;
    assert (mulib_fst_transduce (f, a, input, 2, 4, &result, &work) == MULIB_FST_LIMIT && work == 4);
    assert (result.output == old && result.length == 2);
    assert (mulib_fst_transduce (f, a, input, 3, 0, &result, NULL) == MULIB_FST_NOMATCH);
    assert (result.output == old && result.length == 2);
    assert (mulib_fst_transduce (f, a, NULL, 1, 0, &result, NULL) == MULIB_FST_INVALID);
    assert (mulib_fst_transduce (f, a, input, SIZE_MAX, 0, &result, NULL) == MULIB_FST_LIMIT);
    allocation_count = 0; fail_after = 0;
    assert (mulib_fst_transduce (f, a, input, 2, 0, &result, NULL) == MULIB_FST_NOMEM);
    assert (result.output == old && result.length == 2);
    fail_after = SIZE_MAX;
  }
  assert (mulib_fst_set_accepting (f, b, 1) == MULIB_FST_OK);
  assert (mulib_fst_transduce (f, a, input, 1, 0, &result, NULL) == MULIB_FST_OK && result.final_state == b);
  assert (mulib_fst_set_accepting (f, 2, 1) == MULIB_FST_INVALID);
  mulib_fst_result_free (&result); mulib_fst_result_free (&result); mulib_fst_result_free (NULL);
  mulib_fst_destroy (f); mulib_fst_destroy (NULL);
  allocation_count = 0; fail_after = 0;
  assert (!mulib_fst_create (0, 0));
  fail_after = SIZE_MAX;
  f = mulib_fst_create (0, 0); assert (f);
  allocation_count = 0; fail_after = 0;
  id = 99;
  assert (mulib_fst_add_state (f, 0, &id) == MULIB_FST_NOMEM && id == 99);
  assert (!mulib_fst_state_count (f));
  fail_after = SIZE_MAX;
  assert (mulib_fst_add_state (f, 1, &a) == MULIB_FST_OK);
  allocation_count = 0; fail_after = 0;
  assert (mulib_fst_add_range (f, a, a, 0, 0, 0, 0, &id) == MULIB_FST_NOMEM && id == 99);
  assert (!mulib_fst_edge_count (f));
  fail_after = SIZE_MAX;
  assert (mulib_fst_add_range (f, a, a, 0, 0, 0, 0, &id) == MULIB_FST_OK);
  assert (mulib_fst_transduce (f, a, input, 1, 0, &result, NULL) == MULIB_FST_OK && !result.length);
  mulib_fst_result_free (&result); mulib_fst_destroy (f);
}

static void bad (const char *text, int expected)
{
  mulib_machinebuild_t *m = NULL;
  mulib_machinebuild_diagnostic_t d;
  assert (mulib_machinebuild_parse (text, strlen (text), NULL, &m, &d) == expected);
  assert (!m && d.status == expected && d.message && d.offset <= strlen (text));
  assert (d.line >= 1 && d.column >= 1);
}
static void test_parser (void)
{
  mulib_machinebuild_t *m = NULL, *saved;
  mulib_machinebuild_diagnostic_t d;
  mulib_machinebuild_limits_t limits = {0};
  mulib_fst_result_t result = {0};
  mulib_strbuf_t js, again;
  uint32_t tokens[] = {97, 0, 9, UINT32_MAX, 42};
  assert (mulib_machinebuild_parse (source, sizeof (source) - 1, NULL, &m, &d) == 0);
  assert (m && !d.status && !strcmp (mulib_machinebuild_name (m), "demo"));
  assert (!strcmp (mulib_machinebuild_state_name (m, 1), "active"));
  assert (!mulib_machinebuild_state_name (m, 3));
  assert (mulib_fst_transduce (mulib_machinebuild_fst (m), mulib_machinebuild_start (m),
                              tokens, 5, 0, &result, NULL) == MULIB_FST_OK);
  assert (result.length == 3 && result.output[0] == 65 && result.output[1] == 0
          && result.output[2] == UINT32_MAX && result.final_state == 0);
  mulib_fst_result_free (&result);
  saved = m;
  assert (mulib_machinebuild_parse ("", 0, NULL, &m, &d) == MULIB_MACHINEBUILD_SYNTAX && m == saved);
  assert (mulib_machinebuild_parse (NULL, 0, NULL, &m, &d) == MULIB_MACHINEBUILD_INVALID);
  assert (mulib_machinebuild_parse (source, sizeof (source) - 1, NULL, NULL, NULL) == MULIB_MACHINEBUILD_INVALID);
  limits.source_bytes = 10;
  assert (mulib_machinebuild_parse (source, sizeof (source) - 1, &limits, &m, &d) == MULIB_MACHINEBUILD_LIMIT);
  limits.source_bytes = 0; limits.states = 2;
  assert (mulib_machinebuild_parse (source, sizeof (source) - 1, &limits, &m, &d) == MULIB_MACHINEBUILD_LIMIT);
  limits.states = 0; limits.edges = 3;
  assert (mulib_machinebuild_parse (source, sizeof (source) - 1, &limits, &m, &d) == MULIB_MACHINEBUILD_LIMIT);
  assert (m == saved);
  mulib_strbuf_init (&js); mulib_strbuf_init (&again);
  assert (mulib_strbuf_append (&js, "prefix"));
  assert (mulib_machinebuild_emit_js (m, &js) == 0);
  assert (mulib_machinebuild_emit_js (m, &again) == 0);
  assert (js.length == again.length + 6 && !strcmp (js.data + 6, again.data));
  assert (strstr (js.data, "4294967295") && strstr (js.data, "\"idle\""));
  assert (mulib_machinebuild_emit_js (NULL, &js) == MULIB_MACHINEBUILD_INVALID);
  assert (mulib_machinebuild_emit_js (m, NULL) == MULIB_MACHINEBUILD_INVALID);
  mulib_strbuf_free (&js); mulib_strbuf_free (&again);
  mulib_machinebuild_destroy (m); mulib_machinebuild_destroy (NULL);
  assert (!mulib_machinebuild_fst (NULL) && !mulib_machinebuild_name (NULL)
          && !mulib_machinebuild_state_name (NULL, 0) && mulib_machinebuild_start (NULL) == MULIB_FST_NONE);
  bad ("", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {}", MULIB_MACHINEBUILD_REFERENCE);
  bad ("machine m {state a;}", MULIB_MACHINEBUILD_REFERENCE);
  bad ("machine m {start b; state a;}", MULIB_MACHINEBUILD_REFERENCE);
  bad ("machine m {start a; start a; state a;}", MULIB_MACHINEBUILD_CONFLICT);
  bad ("machine m {state a; state a; start a;}", MULIB_MACHINEBUILD_CONFLICT);
  bad ("machine m {state a; start a; transition a -> b on 1;}", MULIB_MACHINEBUILD_REFERENCE);
  bad ("machine m {state a; start a; transition b -> a on 1;}", MULIB_MACHINEBUILD_REFERENCE);
  bad ("machine m {state a; start a; transition a -> a on 2..1;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a; start a; transition a -> a on 1..2; transition a -> a on 2..3;}",
       MULIB_MACHINEBUILD_CONFLICT);
  bad ("machine m {state a; start a; transition a -> a on 4294967296;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a; start a; transition a -> a on -1;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a; start a; transition a -> a on 1 emit 4294967296;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a; start a; transition a -> a when 1;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a; start a; transition a -> a on 1.2;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a accept start a;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a; start a;} trailing", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a; start a;", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {unknown;}", MULIB_MACHINEBUILD_SYNTAX);
  bad ("machine m {state a$;}", MULIB_MACHINEBUILD_SYNTAX);
  {
    const char exact[] = {'m','a','c','h','i','n','e',' ','m',' ','{','s','t','a','t','e',' ','a',';',
                          's','t','a','r','t',' ','a',';','}'};
    m = NULL;
    assert (!mulib_machinebuild_parse (exact, sizeof (exact), NULL, &m, NULL));
    mulib_machinebuild_destroy (m);
  }
  {
    const char nul[] = "machine m {state a; start a;}\0trailing";
    m = NULL;
    assert (mulib_machinebuild_parse (nul, sizeof (nul) - 1, NULL, &m, &d) == MULIB_MACHINEBUILD_SYNTAX);
  }
  m = NULL;
  assert (mulib_machinebuild_parse ("machine m {\n  wat\n}", 19, NULL, &m, &d) == MULIB_MACHINEBUILD_SYNTAX);
  assert (d.offset == 14 && d.line == 2 && d.column == 3);
  {
    size_t n;
    for (n = 0; n < sizeof (source) - 2; ++n)
      {
        m = NULL;
        /* Any accidentally complete prefix must still own/free its graph. */
        (void)mulib_machinebuild_parse (source, n, NULL, &m, NULL);
        mulib_machinebuild_destroy (m);
      }
  }
  {
    char mutation[sizeof (source)];
    uint32_t random = 17;
    size_t n;
    for (n = 0; n < 2000; ++n)
      {
        memcpy (mutation, source, sizeof (source));
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        mutation[random % (sizeof (source) - 1)] = (char)(random >> 24);
        m = NULL;
        if (!mulib_machinebuild_parse (mutation, sizeof (source) - 1, NULL, &m, &d))
          {
            mulib_strbuf_t text;
            mulib_strbuf_init (&text);
            assert (!mulib_machinebuild_emit_js (m, &text));
            mulib_strbuf_free (&text);
          }
        else assert (!m && d.status && d.offset <= sizeof (source) - 1);
        mulib_machinebuild_destroy (m);
      }
  }
  {
    char identifier[64], text[256];
    memset (identifier, 'a', 63); identifier[63] = 0;
    assert (snprintf (text, sizeof (text), "machine %s {state a; start a;}", identifier) > 0);
    m = NULL;
    assert (!mulib_machinebuild_parse (text, strlen (text), NULL, &m, NULL));
    mulib_machinebuild_destroy (m);
    assert (snprintf (text, sizeof (text), "machine x%s {state a; start a;}", identifier) > 0);
    bad (text, MULIB_MACHINEBUILD_SYNTAX);
  }
}

static void test_allocation_failures (void)
{
  size_t fail;
  mulib_machinebuild_t *m = NULL;
  for (fail = 0; fail < 100; ++fail)
    {
      int status;
      allocation_count = 0; fail_after = fail;
      status = mulib_machinebuild_parse (source, sizeof (source) - 1, NULL, &m, NULL);
      fail_after = SIZE_MAX;
      if (!status) break;
      assert (status == MULIB_MACHINEBUILD_NOMEM && !m);
    }
  assert (m && fail < 100);
  for (fail = 0; fail < 100; ++fail)
    {
      mulib_strbuf_t text;
      int status;
      mulib_strbuf_init (&text); assert (mulib_strbuf_append (&text, "keep"));
      allocation_count = 0; fail_after = fail;
      status = mulib_machinebuild_emit_js (m, &text);
      fail_after = SIZE_MAX;
      if (status) assert (status == MULIB_MACHINEBUILD_NOMEM && text.length == 4 && !strcmp (text.data, "keep"));
      mulib_strbuf_free (&text);
      if (!status) break;
    }
  assert (fail < 100);
  mulib_machinebuild_destroy (m);
}

static void test_growth_and_ownership (void)
{
  mulib_strbuf_t text;
  mulib_machinebuild_t *m = NULL;
  mulib_fst_result_t result = {0};
  char statement[160];
  size_t i, fail;
  uint32_t token = 19;
  mulib_strbuf_init (&text);
  assert (mulib_strbuf_append (&text, "machine growth { start s0;"));
  for (i = 0; i < 20; ++i)
    {
      assert (snprintf (statement, sizeof (statement),
                       "state s%zu accept; transition s0 -> s%zu on %zu emit %zu;", i, i, i, i) > 0);
      assert (mulib_strbuf_append (&text, statement));
    }
  assert (mulib_strbuf_append (&text, "}"));
  for (fail = 0; fail < 100; ++fail)
    {
      int status;
      allocation_count = 0; fail_after = fail;
      status = mulib_machinebuild_parse (text.data, text.length, NULL, &m, NULL);
      fail_after = SIZE_MAX;
      if (!status) break;
      assert (status == MULIB_MACHINEBUILD_NOMEM && !m);
    }
  assert (m && fail < 100);
  mulib_strbuf_free (&text);
  assert (!strcmp (mulib_machinebuild_name (m), "growth"));
  assert (!strcmp (mulib_machinebuild_state_name (m, 19), "s19"));
  assert (mulib_fst_transduce (mulib_machinebuild_fst (m), 0, &token, 1, 0, &result, NULL) == MULIB_FST_OK);
  assert (result.final_state == 19 && result.length == 1 && result.output[0] == 19);
  mulib_machinebuild_destroy (m);
  assert (result.output[0] == 19);
  mulib_fst_result_free (&result);
}

static void emit_reference (void)
{
  const uint32_t alphabet[] = {0, 9, 42, 97, 122, 123, UINT32_MAX};
  mulib_machinebuild_t *m = NULL;
  size_t length, count = 1, i, j;
  int first = 1;
  assert (!mulib_machinebuild_parse (source, sizeof (source) - 1, NULL, &m, NULL));
  assert (putchar ('[') != EOF);
  for (length = 0; length <= 4; ++length, count *= 7)
    for (i = 0; i < count; ++i)
      {
        uint32_t tokens[4];
        size_t n = i;
        mulib_fst_result_t r = {0};
        int status;
        for (j = 0; j < length; ++j) { tokens[j] = alphabet[n % 7]; n /= 7; }
        status = mulib_fst_transduce (mulib_machinebuild_fst (m), mulib_machinebuild_start (m),
                                     tokens, length, 0, &r, NULL);
        assert (status == MULIB_FST_OK || status == MULIB_FST_NOMATCH);
        if (!first) assert (putchar (',') != EOF);
        first = 0;
        assert (printf ("{\"tokens\":[") > 0);
        for (j = 0; j < length; ++j)
          assert (printf ("%s%" PRIu32, j ? "," : "", tokens[j]) > 0);
        assert (printf ("],\"accepted\":%s,\"output\":[", status == MULIB_FST_OK ? "true" : "false") > 0);
        for (j = 0; j < r.length; ++j)
          assert (printf ("%s%" PRIu32, j ? "," : "", r.output[j]) > 0);
        assert (printf ("]}") > 0);
        mulib_fst_result_free (&r);
      }
  assert (puts ("]") >= 0);
  mulib_machinebuild_destroy (m);
}

int main (int argc, char **argv)
{
  if (argc == 2 && !strcmp (argv[1], "--reference")) { emit_reference (); return 0; }
  if (argc == 2 && !strcmp (argv[1], "--emit-js"))
    {
      mulib_machinebuild_t *m = NULL;
      mulib_strbuf_t js;
      mulib_strbuf_init (&js);
      assert (!mulib_machinebuild_parse (source, sizeof (source) - 1, NULL, &m, NULL));
      assert (!mulib_machinebuild_emit_js (m, &js));
      assert (fwrite (js.data, 1, js.length, stdout) == js.length);
      mulib_strbuf_free (&js); mulib_machinebuild_destroy (m);
      return 0;
    }
  test_fst (); test_parser (); test_allocation_failures (); test_growth_and_ownership ();
  return 0;
}
