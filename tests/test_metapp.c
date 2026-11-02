#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long allocation_budget = -1;
static size_t live_allocations;
static void *test_malloc (size_t n) {
  void *p;
  if (!allocation_budget) return NULL;
  if (allocation_budget > 0) --allocation_budget;
  p = malloc (n); if (p) ++live_allocations; return p;
}
static void *test_calloc (size_t n, size_t size) {
  void *p;
  if (!allocation_budget) return NULL;
  if (allocation_budget > 0) --allocation_budget;
  p = calloc (n, size); if (p) ++live_allocations; return p;
}
static void *test_realloc (void *old, size_t n) {
  void *p;
  int was_null = old == NULL;
  if (!allocation_budget) return NULL;
  if (allocation_budget > 0) --allocation_budget;
  p = realloc (old, n); if (p && was_null) ++live_allocations; return p;
}
static void test_free (void *p) {
  if (p) { assert (live_allocations); --live_allocations; }
  free (p);
}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_METAPP
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_LINKER
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DSU
#include "mulib-metapp.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void check (int cpp, const char *source, const char *expected) {
  mulib_metapp_options_t options = {0};
  mulib_metapp_error_t error;
  mulib_strbuf_t output = {0};
  mulib_metapp_t *p;
  int rc;
  options.cplusplus = cpp; p = mulib_metapp_create (&options); assert (p);
  rc = mulib_metapp_process (p, "test.c", source, strlen (source), &output, &error);
  if (rc || strcmp (output.data ? output.data : "", expected)) {
    fprintf (stderr, "source:\n%s\nerror %d at %s:%zu:%zu: %s\nexpected: [%s]\nactual: [%s]\n", source,
      rc, error.file, error.line, error.column, error.message, expected, output.data ? output.data : "");
    assert (0);
  }
  mulib_strbuf_free (&output); mulib_metapp_destroy (p);
}
static void macros (void) {
  check (0, "", "");
  check (0, "#define X 41\nX+1\n", "41 + 1\n");
  check (0, "#define X 41\n#define X 41\n#undef X\nX\n", "X\n");
  check (0, "#define F(x) x+x\nF(F(1))\n", "1 + 1 + 1 + 1\n");
  check (0, "#define A B\n#define B A\nA B A\n", "A B A\n");
  check (0, "#define F(x) F(x)\nF(2)\n", "F ( 2 )\n");
  check (0, "#define F(x) x\n#define A F\nA\n(7)\n", "7\n");
  check (0, "#define S(x) #x\nS( a  + /* note */ b )\nS(\"a\\b\")\n", "\"a + b\" \"\\\"a\\\\b\\\"\"\n");
  check (0, "#define C(a,b) a##b\n#define xy 42\nC(x,y) C(,x) C(x,) C(,)\n", "42 x x\n");
  check (0, "#define V(a,...) a __VA_ARGS__\nV(1,+2,+3) V(4)\n", "1 + 2 , + 3 4\n");
  check (1, "#define E\n#define V(...) f(0 __VA_OPT__(,) __VA_ARGS__)\nV() V(1,2) V(E)\n", "f ( 0 ) f ( 0 , 1 , 2 ) f ( 0 )\n");
  check (0, "#define BAD(x,y) x+y\n#define S(x) #x\nS(BAD(1))\n", "\"BAD(1)\"\n");
  check (0, "#define I(x) x\nI(a ## b)\n", "a ## b\n");
  check (0, "#define hash_hash # ## #\n#define mkstr(a) # a\n#define in_between(a) mkstr(a)\n#define join(c,d) in_between(c hash_hash d)\njoin(x,y)\n", "\"x ## y\"\n");
  check (0, "#define X (1)\n#define F (x)\nF X\n", "( x ) ( 1 )\n");
  check (0, "#define ADD(a,b) a+b\nADD((1,2),3)\n", "( 1 , 2 ) + 3\n");
  check (0, "#define F(x) x\n#define G F\nG(G)(2)\n", "F ( 2 )\n");
  check (0, "#define S(x) #x\n#define I(x) S(x)\n#define E\nI(a E+b)\n", "\"a +b\"\n");
  check (0, "#define S(x) #x\nS(<: %: %:%:)\n", "\"<: %: %:%:\"\n");
  check (1, "#define X 1\n#define F(a,...) #__VA_OPT__(a + __VA_ARGS__)\nF(X,x) F(X)\n", "\"1 + x\" \"\"\n");
  check (1, "#define X 1\n#define F(a,...) #__VA_OPT__(a ## a)\nF(X,x)\n", "\"XX\"\n");
}
static void conditional (void) {
  check (0, "#define X 4\n#if defined X && defined(X) && 1+2*3==7\nyes\n#else\nno\n#endif\n", "yes\n");
  check (0, "#if 0\n#if 1/0\nno\n#endif\n#elif 1\nyes\n#elif 1/0\nno\n#endif\n", "yes\n");
  check (0, "#if (0 && 1/0) || (1 ? 1 : 1/0)\nyes\n#endif\n", "yes\n");
  check (0, "#if -1 < 1U\nno\n#else\nyes\n#endif\n", "yes\n");
  check (0, "#if (1 ? -1 : 1U) > 0 && (8>>2)==2 && (~0U)>0 && '\\x41'==65\nyes\n#endif\n", "yes\n");
  check (1, "#if true and not false and ((7 bitand 3) == 3) and (2 not_eq 3)\nyes\n#endif\n", "yes\n");
  check (0, "#ifdef MISSING\nno\n#elifndef MISSING\nyes\n#endif\n", "yes\n");
  check (0, "#if UNKNOWN || 0\n#bad ignored\n#else\nyes\n#endif\n", "yes\n");
  check (0, "#if 0xffffffffffffffffULL == -1 && (1<<3)==8 && (-8>>2)==-2\nyes\n#endif\n", "yes\n");
  check (0, "#if 1 ? 0 ? 0 : 1 : 0\nyes\n#endif\n", "yes\n");
}
static void lexical (void) {
  check (0, "#define X a\\\nb\nX __LINE__\n", "ab 3\n");
  check (0, "#line 700 \"logical.c\"\n__LINE__ __FILE__\n__COUNTER__ __COUNTER__\n", "700 \"logical.c\" 0 1\n");
  check (0, "%:define C(a,b) a %:%: b\nC(x,y) <:1:> <%2%>\n", "xy <: 1 :> <% 2 %>\n");
  check (1, "R\"tag(a\\\nb/*c*/\"d)tag\" u8\"str\"_suffix L'x'\n", "R\"tag(a\\\nb/*c*/\"d)tag\" u8\"str\"_suffix L'x'\n");
  check (0, "#define \\u03b1 5\nα\n", "5\n");
  check (0, "#define X a/*\ncomment*/b\nX __LINE__\n", "a b 3\n");
  check (0, "#define X 1\r\nX\r\n", "1\n");
  check (0, "__DATE__ __TIME__ __STDC_VERSION__\n", "\"Jan  1 1970\" \"00:00:00\" 201710L\n");
  check (1, "__cplusplus\n", "202002L\n");
  check (0, "before\n_Pragma(\"pack(push,1)\")\nafter\n", "before\n#pragma pack ( push , 1 )\n after\n");
  check (1, "#define S(x) #x\nS(R\"(a\nb)\")\n", "\"R\\\"(a\\nb)\\\"\"\n");
  check (1, "X<::A>\n", "X < :: A >\n");
  check (0, "#line 4 \"a\\\\b\\\"c\"\n__FILE__ __LINE__\n", "\"a\\\\b\\\"c\" 4\n");
  check (0, "#line 8 \"\"\n__FILE__\n", "\"\"\n");
}
static void malformed (void) {
  static const char *const sources[] = {
    "#else\n", "#if 1\n", "#if 1\n#else\n#else\n#endif\n", "#if 1/0\n#endif\n",
    "#if 1<<64\n#endif\n", "#if 0x\n#endif\n", "#if 08\n#endif\n", "#if 1.2\n#endif\n",
    "#if 1 ? 2\n#endif\n", "#if defined()\n#endif\n", "#define F(x,x) x\n",
    "#define F(x) #y\n", "#define F(x) ##x\n", "#define F(x) x##\n", "#define F(x) x\nF(1,2)\n",
    "#define F(x) x\nF(1\n", "#define F(a,b) a##b\nF(+,*)\n", "#define F __VA_ARGS__\n",
    "#define F(...) __VA_OPT__(1\n", "#define F(...) __VA_OPT__(__VA_OPT__(x))\n",
    "#define F(...) __VA_OPT__(##x)\n", "#define F(x) a ## ## x\n",
    "#define X 1\n#define X 2\n", "#undef __LINE__\n", "#error failure\n",
    "\"unfinished", "/*unfinished", "#unrecognized value\n", "#line 0\n", "#include MISSING\n"
  };
  size_t i;
  for (i = 0; i < sizeof (sources)/sizeof (sources[0]); ++i) {
    mulib_metapp_t *p = mulib_metapp_create (NULL);
    mulib_strbuf_t output = {0}; mulib_metapp_error_t error;
    assert (p && mulib_strbuf_append (&output, "untouched"));
    if (!mulib_metapp_process (p, "bad.c", sources[i], strlen (sources[i]), &output, &error)) {
      fprintf (stderr, "accepted malformed input: %s\n", sources[i]); assert (0);
    }
    assert (error.code && error.message[0] && !strcmp (output.data, "untouched"));
    mulib_strbuf_free (&output); mulib_metapp_destroy (p);
  }
}
typedef struct files { int opened, released; } files;
static void release_source (void *user) { ++((files *)user)->released; }
static int include (void *user, const char *from, const char *header, int angled, mulib_metapp_source_t *out) {
  files *fs = (files *)user;
  (void)from; (void)angled;
  if (strcmp (header, "config.h") && strcmp (header, "recursive.h")) return MULIB_METAPP_IO;
  out->name = !strcmp (header, "config.h") ? "memory:config" : "memory:recursive";
  out->data = !strcmp (header, "config.h") ? "#pragma once\n#define VALUE 9\nheader VALUE\n" :
    "#ifndef RECURSIVE_H\n#define RECURSIVE_H\n#include \"recursive.h\"\nrecursive\n#endif\n";
  out->length = strlen (out->data); out->release = release_source; out->user = user;
  ++fs->opened; return 0;
}
static void includes (void) {
  const char *src = "#define H \"config.h\"\n#define config absent\n#include H\n#include <config.h>\n"
    "#include \"recursive.h\"\n#if __has_include(\"config.h\") && !__has_include(<absent.h>)\nVALUE\n#endif\n";
  mulib_metapp_t *p = mulib_metapp_create (NULL);
  mulib_strbuf_t output = {0}; mulib_metapp_error_t error;
  files fs = {0,0};
  assert (p && !mulib_metapp_include_resolver (p, include, &fs));
  assert (!mulib_metapp_process (p, "memory:main", src, strlen (src), &output, &error));
  assert (!strcmp (output.data, "header 9\nrecursive\n9\n"));
  assert (fs.opened == 5 && fs.released == 5);
  assert (mulib_metapp_defined (p, "VALUE"));
  mulib_strbuf_free (&output); mulib_metapp_destroy (p);
}
static int warned;
static void warning (void *user, const mulib_metapp_error_t *error) {
  assert (mulib_metapp_define (*(mulib_metapp_t **)user, "X 1", NULL) == MULIB_METAPP_BUSY);
  assert (error->line == 31 && !strcmp (error->file, "logical.c") && !strcmp (error->message, "notice"));
  ++warned;
}
static int unusual_header (void *user, const char *from, const char *header, int angled, mulib_metapp_source_t *out) {
  (void)user; (void)from;
  assert (angled && !strcmp (header, "path//odd'name.h"));
  out->data = "included"; out->length = 8; out->name = "virtual:header"; return 0;
}
static void state_and_diagnostics (void) {
  mulib_metapp_options_t options = {0};
  mulib_metapp_t *p;
  mulib_strbuf_t output = {0}; mulib_metapp_error_t error;
  const char *source = "?" "?=define X 1?" "?/\n2\nX __LINE__\n";
  options.trigraphs = 1; options.warning = warning; options.warning_user = &p;
  p = mulib_metapp_create (&options); assert (p);
  assert (!mulib_metapp_process (p, "trigraph.c", source, strlen (source), &output, &error));
  assert (!strcmp (output.data, "12 3\n")); mulib_strbuf_free (&output);
  source = "#line 31 \"logical.c\"\n#warning notice\n#define SAVED 9\n#error bad\n";
  assert (mulib_metapp_process (p, "original.c", source, strlen (source), &output, &error) == MULIB_METAPP_SYNTAX);
  assert (warned == 1 && error.line == 33 && !strcmp (error.file, "logical.c") && !strcmp (error.message, "bad"));
  assert (mulib_metapp_defined (p, "SAVED"));
  assert (!mulib_metapp_process (p, "reuse.c", "SAVED", 5, &output, &error) && !strcmp (output.data, "9\n"));
  mulib_strbuf_free (&output);
  assert (!mulib_metapp_include_resolver (p, unusual_header, NULL));
  source = "#include <path//odd'name.h>\n";
  assert (!mulib_metapp_process (p, "main.c", source, strlen (source), &output, &error) && !strcmp (output.data, "included\n"));
  mulib_strbuf_free (&output); mulib_metapp_destroy (p);
}
static int destroyed;
static void destroy_plugin (void *user) { assert (user == &destroyed); ++destroyed; }
static int static_plugin (void *user, const mulib_metapp_host_t *host, mulib_metapp_t *p,
                          const mulib_metapp_event_t *event, mulib_strbuf_t *output) {
  (void)user;
  if (event->kind == MULIB_METAPP_DIRECTIVE && !strcmp (event->name, "set"))
    return host->define (p, event->arguments, NULL) ? -1 : 1;
  if (event->kind == MULIB_METAPP_DIRECTIVE && !strcmp (event->name, "emit"))
    return host->append (output, event->arguments, strlen (event->arguments)) ? 1 : -1;
  if (event->kind == MULIB_METAPP_PRAGMA && !strcmp (event->name, "set"))
    return host->define (p, event->arguments, NULL) ? -1 : 1;
  if (event->kind == MULIB_METAPP_PRAGMA && !strcmp (event->name, "unset"))
    return host->undefine (p, event->arguments) ? -1 : 1;
  if (event->kind == MULIB_METAPP_IDENTIFIER && !strcmp (event->name, "MAGIC")) {
    assert (host->define (p, "UNSAFE 1", NULL) == MULIB_METAPP_BUSY);
    return host->append (output, "MAGIC + X", 9) ? 1 : -1;
  }
  return 0;
}
static void plugins (void) {
  const char src[] = "#pragma once\n#set X 42\n#emit MAGIC\n"
    "X _Pragma(\"unset X\") _Pragma(\"set X 7\") X\n"
    "#define ID(x) x x\nID(_Pragma(L\"unset ID\") value)\nID\n";
  mulib_metapp_t *p = mulib_metapp_create (NULL);
  mulib_metapp_plugin_t plugin = {MULIB_METAPP_PLUGIN_ABI, sizeof (plugin), "test", static_plugin, &destroyed, destroy_plugin};
  mulib_strbuf_t output = {0}; mulib_metapp_error_t error;
  assert (p && !mulib_metapp_plugin (p, &plugin));
  assert (mulib_metapp_plugin (p, &plugin) == MULIB_METAPP_INVALID);
  assert (!mulib_metapp_process (p, "test.c", src, sizeof (src)-1, &output, &error));
  assert (!strcmp (output.data, "MAGIC + 42\n42 7\nvalue value ID\n"));
  assert (mulib_metapp_plugin_link (p, "bad", 3, &error) < 0);
  assert (!destroyed); mulib_metapp_destroy (p); assert (destroyed == 1);
  mulib_strbuf_free (&output);
}
static void limits (void) {
  mulib_metapp_options_t options = {0};
  mulib_metapp_error_t error;
  mulib_metapp_t *p;
  mulib_strbuf_t output = {0};
  options.output_bytes = 2; p = mulib_metapp_create (&options); assert (p);
  assert (mulib_metapp_process (p, "limit.c", "abcd", 4, &output, &error) == MULIB_METAPP_LIMIT && !output.length);
  mulib_metapp_destroy (p);
  memset (&options, 0, sizeof (options)); options.tokens = 8; p = mulib_metapp_create (&options); assert (p);
  assert (mulib_metapp_process (p, "limit.c", "1 2 3 4 5 6 7 8 9", 17, &output, &error) == MULIB_METAPP_LIMIT);
  mulib_metapp_destroy (p);
  memset (&options, 0, sizeof (options)); options.macros = 1; p = mulib_metapp_create (&options); assert (p);
  assert (!mulib_metapp_define (p, "X 1", &error));
  assert (mulib_metapp_define (p, "Y 2", &error) == MULIB_METAPP_LIMIT);
  assert (mulib_metapp_defined (p, "X") && !mulib_metapp_defined (p, "Y"));
  assert (!mulib_metapp_undefine (p, "X") && !mulib_metapp_define (p, "Y 2", &error));
  assert (mulib_metapp_process (p, "bad.c", "\0", 1, &output, &error) == MULIB_METAPP_SYNTAX);
  mulib_metapp_destroy (p); mulib_strbuf_free (&output);
}
static void allocation_failures (void) {
  const char source[] = "#define ID(a) a\n#define CAT(a,b) a##b\n#define STR(x) #x\n"
    "#define V(...) __VA_OPT__(STR(__VA_ARGS__))\nID(CAT(4,2)) V(one+two)\n";
  long budget;
  int complete = 0, variant;
  for (budget = 0; budget < 4; ++budget) {
    mulib_metapp_t *p;
    allocation_budget = budget; p = mulib_metapp_create (NULL); allocation_budget = -1;
    if (p) mulib_metapp_destroy (p);
    assert (!live_allocations);
  }
  for (variant = 0; variant < 2; ++variant) {
   complete = 0;
   for (budget = 0; budget < 2000 && !complete; ++budget) {
    mulib_metapp_t *p = mulib_metapp_create (NULL);
    mulib_strbuf_t output = {0}; mulib_metapp_error_t error;
    files fs = {0,0};
    const char *input = variant ? "#include \"config.h\"\nVALUE\n" : source;
    int rc;
    assert (p && mulib_strbuf_append (&output, "prefix:"));
    assert (!mulib_metapp_include_resolver (p, include, &fs));
    allocation_budget = budget;
    rc = mulib_metapp_process (p, "alloc.c", input, strlen (input), &output, &error);
    allocation_budget = -1;
    assert (rc == error.code);
    if (rc) assert (!strcmp (output.data, "prefix:") && rc == MULIB_METAPP_MEMORY);
    else complete = 1;
    mulib_strbuf_free (&output); mulib_metapp_destroy (p);
    assert (!live_allocations && fs.opened == fs.released);
   }
   assert (complete);
  }
}
typedef struct dependency_files {
  int opened, released, destroyed, generated;
  mulib_metapp_t *owner;
} dependency_files;
static void dependency_release (void *user) { ++((dependency_files *)user)->released; }
static int dependency_include (void *user, const char *from, const char *header, int angled, mulib_metapp_source_t *out) {
  static const struct { const char *name, *source; } files[] = {
    {"mem/a.c", "#define LOCAL 1\n#line 800 \"logical.c\"\n#include SELECT(\"common.h\")\n#depinclude\n"
      "#if 0\n#include \"missing.h\"\n#endif\n#if __has_include(\"probe.h\")\nint probe;\n#endif\n"
      "const char *s = \"#include \\\"not-real.h\\\"\";\n"},
    {"mem/z.c", "#include \"common.h\"\n#include \"common.h\"\n#include \"bridge.h\"\n"},
    {"mem/unrelated.c", "int unrelated;\n"},
    {"mem/common.h", "#pragma once\n#ifdef LOCAL\n#include \"local.h\"\n#else\n#include \"other.h\"\n#endif\n#include \"leaf.h\"\n"},
    {"mem/local.h", ""}, {"mem/other.h", ""},
    {"mem/leaf.h", "#ifndef LEAF_H\n#define LEAF_H\n#include \"cycle.h\"\n#endif\n"},
    {"mem/cycle.h", "#include \"leaf.h\"\n"},
    {"mem/bridge.h", "#include \"common.h\"\n"},
    {"mem/plugin.h", "#include \"leaf.h\"\n"},
    {"mem/probe.h", "probe"}, {"mem/bad.c", "#include \"missing.h\"\n"},
    {"mem/loop.h", "#include \"loop.h\"\n"}
  };
  dependency_files *fs = (dependency_files *)user;
  size_t i;
  (void)angled;
  if (fs->owner) {
    mulib_metapp_graph_t *g = NULL;
    assert (mulib_metapp_dependencies (fs->owner, NULL, 0, NULL, &g, NULL) == MULIB_METAPP_BUSY && !g);
    assert (mulib_metapp_define (fs->owner, "MUTATE 1", NULL) == MULIB_METAPP_BUSY);
  }
  for (i = 0; i < sizeof (files)/sizeof (files[0]); ++i) {
    const char *name = from ? strrchr (files[i].name, '/')+1 : files[i].name;
    if (strcmp (name, header)) continue;
    out->name = files[i].name; out->data = files[i].source; out->length = strlen (out->data);
    out->release = dependency_release; out->user = user; ++fs->opened; return 0;
  }
  return MULIB_METAPP_IO;
}
static void dependency_destroy (void *user) { ++((dependency_files *)user)->destroyed; }
static int dependency_plugin (void *user, const mulib_metapp_host_t *host, mulib_metapp_t *p,
                              const mulib_metapp_event_t *event, mulib_strbuf_t *out) {
  const char source[] = "#include \"plugin.h\"\n";
  (void)p;
  if (event->kind == MULIB_METAPP_DIRECTIVE && !strcmp (event->name, "depinclude")) {
    ++((dependency_files *)user)->generated;
    return host->append (out, source, sizeof (source)-1) ? 1 : -1;
  }
  return 0;
}
static mulib_metapp_t *dependency_context (dependency_files *fs) {
  mulib_metapp_options_t options = {0};
  mulib_metapp_t *p;
  mulib_metapp_plugin_t plugin = {MULIB_METAPP_PLUGIN_ABI, sizeof (plugin), "dependency", dependency_plugin, fs, dependency_destroy};
  options.include_depth = 8;
  p = mulib_metapp_create (&options); assert (p);
  assert (!mulib_metapp_define (p, "SELECT(x) x", NULL));
  assert (!mulib_metapp_define (p, "EMPTY", NULL));
  assert (!mulib_metapp_include_path (p, "unused"));
  assert (!mulib_metapp_include_resolver (p, dependency_include, fs));
  assert (!mulib_metapp_plugin (p, &plugin)); fs->owner = p;
  return p;
}
static int dependency_edge (const mulib_metapp_graph_t *g, const char *from, const char *to) {
  size_t i;
  for (i = 0; i < mulib_metapp_graph_edge_count (g); ++i) {
    const mulib_metapp_dependency_t *e = mulib_metapp_graph_edge (g, i);
    if (!strcmp (mulib_metapp_graph_file (g, e->from), from) && !strcmp (mulib_metapp_graph_file (g, e->to), to)) return 1;
  }
  return 0;
}
static void dependency_graphs (void) {
  const char *roots[] = {"mem/z.c", "mem/unrelated.c", "mem/a.c", "mem/a.c"};
  const char *reordered[] = {"mem/a.c", "mem/z.c", "mem/unrelated.c"};
  const char *bad = "mem/bad.c", *loop = "mem/loop.h";
  dependency_files fs = {0};
  mulib_metapp_t *p = dependency_context (&fs);
  mulib_metapp_graph_t *g = NULL, *other = NULL, *component = NULL;
  mulib_metapp_error_t error;
  mulib_metapp_dependency_options_t limits = {0};
  mulib_strbuf_t expected = {0}, whole = {0}, again = {0};
  size_t i;
  assert (!mulib_metapp_dependencies (p, roots, 4, NULL, &g, &error));
  assert (!error.code && !fs.destroyed && fs.generated == 1 && fs.opened == fs.released);
  assert (mulib_metapp_defined (p, "SELECT") && mulib_metapp_defined (p, "EMPTY"));
  assert (!mulib_metapp_defined (p, "LOCAL") && !mulib_metapp_defined (p, "LEAF_H"));
  assert (mulib_metapp_graph_count (g) == 10 && mulib_metapp_graph_edge_count (g) == 11);
  assert (dependency_edge (g, "mem/bridge.h", "mem/common.h")); /* once-skipped include */
  assert (dependency_edge (g, "mem/cycle.h", "mem/leaf.h"));  /* guarded cycle */
  assert (dependency_edge (g, "mem/common.h", "mem/local.h") && dependency_edge (g, "mem/common.h", "mem/other.h"));
  assert (dependency_edge (g, "mem/a.c", "mem/plugin.h"));
  assert (!mulib_metapp_graph_file (g, 10) && !mulib_metapp_graph_edge (g, 11));
  assert (!mulib_metapp_graph_dot (g, &whole));
  assert (!strstr (whole.data, "probe.h") && !strstr (whole.data, "missing.h") && !strstr (whole.data, "logical.c"));
  assert (!mulib_metapp_graph_component (g, "mem/leaf.h", &component));
  assert (mulib_metapp_graph_count (component) == 9 && mulib_metapp_graph_edge_count (component) == 11);
  assert (!mulib_metapp_graph_dot (component, &expected));
  mulib_metapp_graph_destroy (component); component = NULL;
  for (i = 0; i < mulib_metapp_graph_count (g); ++i) {
    const char *file = mulib_metapp_graph_file (g, i);
    if (i) assert (strcmp (mulib_metapp_graph_file (g, i-1), file) < 0);
    if (!strcmp (file, "mem/unrelated.c")) continue;
    assert (!mulib_metapp_graph_component (g, file, &component));
    assert (!mulib_metapp_graph_dot (component, &again) && !strcmp (again.data, expected.data));
    mulib_strbuf_free (&again); mulib_metapp_graph_destroy (component); component = NULL;
  }
  assert (!mulib_metapp_dependencies (p, reordered, 3, NULL, &other, &error));
  assert (!mulib_metapp_graph_dot (other, &again) && !strcmp (again.data, whole.data));
  mulib_strbuf_free (&again); mulib_metapp_graph_destroy (other); other = NULL;
  assert (mulib_metapp_graph_component (g, "mem/absent.h", &component) == MULIB_METAPP_NOTFOUND && !component);
  assert (!mulib_metapp_graph_component (g, "mem/unrelated.c", &component));
  assert (mulib_metapp_graph_count (component) == 1 && !mulib_metapp_graph_edge_count (component));
  assert (mulib_metapp_graph_component (g, "mem/a.c", &component) == MULIB_METAPP_INVALID);
  mulib_metapp_graph_destroy (component); component = NULL;
  assert (!mulib_metapp_dependencies (p, NULL, 0, NULL, &other, &error));
  assert (!mulib_metapp_graph_count (other) && !mulib_metapp_graph_edge_count (other));
  assert (mulib_metapp_graph_component (other, "mem/a.c", &component) == MULIB_METAPP_NOTFOUND);
  mulib_metapp_graph_destroy (other); other = NULL;
  assert (mulib_metapp_dependencies (p, &bad, 1, NULL, &other, &error) == MULIB_METAPP_IO && !other);
  assert (error.code == MULIB_METAPP_IO && !strcmp (error.file, bad));
  assert (mulib_metapp_dependencies (p, &loop, 1, NULL, &other, &error) == MULIB_METAPP_LIMIT && !other);
  limits.max_files = 2;
  assert (mulib_metapp_dependencies (p, roots, 1, &limits, &other, &error) == MULIB_METAPP_LIMIT && !other);
  limits.max_files = 0; limits.max_edges = 1;
  assert (mulib_metapp_dependencies (p, roots, 1, &limits, &other, &error) == MULIB_METAPP_LIMIT && !other);
  assert (fs.opened == fs.released);
  assert (!mulib_metapp_graph_component (g, "mem/common.h", &component));
  mulib_metapp_graph_destroy (g); mulib_metapp_destroy (p); fs.owner = NULL;
  assert (fs.destroyed == 1);
  assert (!mulib_metapp_graph_component (component, "mem/z.c", &other));
  assert (!mulib_metapp_graph_dot (other, &again) && !strcmp (again.data, expected.data));
  mulib_metapp_graph_destroy (component); mulib_metapp_graph_destroy (other);
  mulib_strbuf_free (&whole); mulib_strbuf_free (&again); mulib_strbuf_free (&expected);
}
static void dependency_allocation_failures (void) {
  const char *roots[] = {"mem/a.c", "mem/z.c"};
  dependency_files fs = {0};
  mulib_metapp_t *p = dependency_context (&fs);
  mulib_metapp_graph_t *g = NULL, *part = NULL;
  mulib_metapp_error_t error;
  size_t baseline = live_allocations;
  long budget;
  int complete = 0, rc;
  for (budget = 0; budget < 2000 && !complete; ++budget) {
    allocation_budget = budget;
    rc = mulib_metapp_dependencies (p, roots, 2, NULL, &g, &error);
    allocation_budget = -1;
    /* An allocation failure reported by the extension uses the plugin status. */
    assert (rc == error.code && (rc == 0 || rc == MULIB_METAPP_MEMORY || rc == MULIB_METAPP_PLUGIN));
    if (!rc) complete = 1; else assert (!g);
    mulib_metapp_graph_destroy (g); g = NULL;
    assert (live_allocations == baseline && fs.opened == fs.released && !fs.destroyed);
  }
  assert (complete && !mulib_metapp_dependencies (p, roots, 2, NULL, &g, &error));
  baseline = live_allocations; complete = 0;
  for (budget = 0; budget < 200 && !complete; ++budget) {
    allocation_budget = budget; rc = mulib_metapp_graph_component (g, "mem/leaf.h", &part); allocation_budget = -1;
    assert (rc == 0 || rc == MULIB_METAPP_MEMORY);
    if (!rc) complete = 1; else assert (!part);
    mulib_metapp_graph_destroy (part); part = NULL; assert (live_allocations == baseline);
  }
  assert (complete); complete = 0;
  for (budget = 0; budget < 100 && !complete; ++budget) {
    mulib_strbuf_t text = {0}; assert (mulib_strbuf_append (&text, "prefix:"));
    allocation_budget = budget; rc = mulib_metapp_graph_dot (g, &text); allocation_budget = -1;
    assert (rc == 0 || rc == MULIB_METAPP_MEMORY);
    if (!rc) complete = 1; else assert (!strcmp (text.data, "prefix:"));
    mulib_strbuf_free (&text); assert (live_allocations == baseline);
  }
  assert (complete); mulib_metapp_graph_destroy (g); mulib_metapp_destroy (p);
  assert (!live_allocations && fs.destroyed == 1);
}
static int dependency_tree (const char *directory, const char *file) {
  mulib_metapp_t *p = mulib_metapp_create (NULL);
  mulib_metapp_graph_t *g = NULL, *part = NULL;
  mulib_metapp_error_t error = {0}; mulib_strbuf_t text = {0};
  int rc;
  assert (p); rc = mulib_metapp_dependencies_tree (p, directory, NULL, &g, &error);
  if (!rc) rc = mulib_metapp_graph_component (g, file, &part);
  if (!rc) rc = mulib_metapp_graph_dot (part, &text);
  if (!rc) assert (fwrite (text.data, 1, text.length, stdout) == text.length);
  else fprintf (stderr, "dependency graph: %s:%zu: %s (%d)\n", error.file, error.line, error.message, rc);
  mulib_strbuf_free (&text); mulib_metapp_graph_destroy (part); mulib_metapp_graph_destroy (g); mulib_metapp_destroy (p);
  assert (!live_allocations); return rc ? 1 : 0;
}
static int dependency_tree_failures (const char *directory) {
  mulib_metapp_t *p = mulib_metapp_create (NULL);
  mulib_metapp_graph_t *g = NULL;
  mulib_metapp_dependency_options_t limits = {0};
  mulib_metapp_error_t error;
  size_t baseline;
  long budget;
  int complete = 0, rc;
  assert (p); baseline = live_allocations;
  limits.max_entries = 1;
  assert (mulib_metapp_dependencies_tree (p, directory, &limits, &g, &error) == MULIB_METAPP_LIMIT && !g);
  limits.max_entries = 0; limits.max_files = 1;
  assert (mulib_metapp_dependencies_tree (p, directory, &limits, &g, &error) == MULIB_METAPP_LIMIT && !g);
  limits.max_files = 0; limits.max_edges = 1;
  assert (mulib_metapp_dependencies_tree (p, directory, &limits, &g, &error) == MULIB_METAPP_LIMIT && !g);
  for (budget = 0; budget < 2000 && !complete; ++budget) {
    allocation_budget = budget; rc = mulib_metapp_dependencies_tree (p, directory, NULL, &g, &error); allocation_budget = -1;
    assert (rc == error.code && (rc == 0 || rc == MULIB_METAPP_MEMORY));
    if (!rc) complete = 1; else assert (!g);
    mulib_metapp_graph_destroy (g); g = NULL; assert (live_allocations == baseline);
  }
  assert (complete); mulib_metapp_destroy (p); assert (!live_allocations); return 0;
}
static int native_plugin (const char *path) {
  const char source[] = "#metapp_define VALUE 40\n#metapp_emit VALUE + __PLUGIN_COUNT__\n"
    "#pragma metapp_note ignored\n__PLUGIN_COUNT__ __PLUGIN_COUNT__\n";
  mulib_metapp_t *p = mulib_metapp_create (NULL);
  mulib_metapp_error_t error; mulib_strbuf_t output = {0};
  int rc;
  assert (p); rc = mulib_metapp_plugin_file (p, path, &error);
  if (rc) { fprintf (stderr, "plugin: %s (%d)\n", error.message, rc); mulib_metapp_destroy (p); return 1; }
  assert (!mulib_metapp_process (p, "native.c", source, sizeof (source)-1, &output, &error));
  assert (!strcmp (output.data, "40 + 0\n1 2\n"));
  assert (mulib_metapp_plugin_file (p, path, &error) == MULIB_METAPP_INVALID);
  mulib_strbuf_free (&output); mulib_metapp_destroy (p);
  {
    long budget; int complete = 0;
    for (budget = 0; budget < 2000 && !complete; ++budget) {
      p = mulib_metapp_create (NULL); assert (p);
      allocation_budget = budget; rc = mulib_metapp_plugin_file (p, path, &error); allocation_budget = -1;
      assert (rc == error.code && (rc == 0 || rc == MULIB_METAPP_MEMORY));
      if (!rc) complete = 1;
      mulib_metapp_destroy (p); assert (!live_allocations);
    }
    assert (complete);
  }
  return 0;
}
int main (int argc, char **argv) {
  if (argc == 4 && !strcmp (argv[1], "--graph")) return dependency_tree (argv[2], argv[3]);
  if (argc == 3 && !strcmp (argv[1], "--graph-check")) return dependency_tree_failures (argv[2]);
  if (argc == 3 && !strcmp (argv[1], "--native")) return native_plugin (argv[2]);
  if (argc == 3 && !strcmp (argv[1], "--reject")) {
    mulib_metapp_t *p = mulib_metapp_create (NULL); mulib_metapp_error_t error; int rc;
    assert (p); rc = mulib_metapp_plugin_file (p, argv[2], &error);
    if (!rc) { fprintf (stderr, "accepted invalid ELF: %s\n", argv[2]); assert (0); }
    assert (rc == error.code && error.message[0]);
    mulib_metapp_destroy (p); assert (!live_allocations); return 0;
  }
  if (argc == 3 && (!strcmp (argv[1], "--file") || !strcmp (argv[1], "--cpp"))) {
    mulib_metapp_options_t options = {0}; mulib_metapp_t *p;
    mulib_metapp_error_t error; mulib_strbuf_t output = {0}; int rc;
    options.cplusplus = !strcmp (argv[1], "--cpp"); p = mulib_metapp_create (&options); assert (p);
    rc = mulib_metapp_file (p, argv[2], &output, &error);
    if (rc) fprintf (stderr, "%s:%zu:%zu: %s (%d)\n", error.file, error.line, error.column, error.message, rc);
    else if (output.length) assert (fwrite (output.data, 1, output.length, stdout) == output.length);
    mulib_strbuf_free (&output); mulib_metapp_destroy (p); return rc ? 1 : 0;
  }
  macros (); conditional (); lexical (); malformed (); includes (); state_and_diagnostics (); plugins (); limits (); allocation_failures ();
  dependency_graphs (); dependency_allocation_failures ();
  assert (!live_allocations);
  return 0;
}
