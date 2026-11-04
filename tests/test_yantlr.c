#define MULIB_IMPL_ARENA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_YANTLR
#include "mulib-yantlr.h"
#include <assert.h>

static int calls, destroyed;
static int sequence[16];
static int identity(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *const *a,
    size_t n, const mulib_yantlr_value_t **out, void *u) {
  (void)r; (void)u; assert(n == 1);
  if (calls < 16) sequence[calls] = (int)mulib_yantlr_numeric(a[0]);
  ++calls; *out = a[0]; return 1;
}
static int add(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *const *a,
    size_t n, const mulib_yantlr_value_t **out, void *u) {
  (void)u; assert(n == 2);
  if (mulib_yantlr_typeof(a[0]) != MULIB_YANTLR_NUMBER || mulib_yantlr_typeof(a[1]) != MULIB_YANTLR_NUMBER)
    return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "add expects numbers");
  *out = mulib_yantlr_number(r, mulib_yantlr_numeric(a[0]) + mulib_yantlr_numeric(a[1])); return *out != NULL;
}
static int geq(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *const *a,
    size_t n, const mulib_yantlr_value_t **out, void *u) {
  (void)u; assert(n == 2);
  *out = mulib_yantlr_bool(r, mulib_yantlr_numeric(a[0]) >= mulib_yantlr_numeric(a[1])); return *out != NULL;
}
static void release(void *p) { ++destroyed; free(p); }
static int generate(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *const *a,
    size_t n, const mulib_yantlr_value_t **out, void *u) {
  const char *source = "int main(void) { return 0; }\n";
  void *p = malloc(1);
  (void)u; assert(n == 1);
  if (!p) return 0;
  *out = mulib_yantlr_opaque(r, "test.generator", p, release);
  if (!*out) { free(p); return 0; }
  if (!mulib_yantlr_emit(r, "generated/main.c", source, strlen(source))) return 0;
  if (!mulib_yantlr_emit(r, "generated/api.h", NULL, 0)) return 0;
  assert(mulib_yantlr_typeof(a[0]) == MULIB_YANTLR_RECORD);
  return 1;
}
static int broken(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *const *a,
    size_t n, const mulib_yantlr_value_t **out, void *u) {
  (void)a; (void)n; (void)out; (void)u;
  assert(mulib_yantlr_emit(r, "temporary.c", "discard", 7));
  return mulib_yantlr_fail(r, MULIB_YANTLR_PROVIDER, "intentional failure");
}
static mulib_yantlr_module_t *parse(const char *s) {
  mulib_yantlr_error_t e;
  mulib_yantlr_module_t *m = mulib_yantlr_parse(s, strlen(s), NULL, &e);
  if (!m) fprintf(stderr, "%zu:%zu: %s\n", e.line, e.column, e.message);
  assert(m && e.code == MULIB_YANTLR_OK); return m;
}
static void malformed(const char *s, mulib_yantlr_error_code_t code) {
  mulib_yantlr_error_t e;
  assert(!mulib_yantlr_parse(s, strlen(s), NULL, &e));
  if (e.code != code) fprintf(stderr, "expected error %d, got %d: %s: %s\n", (int)code, (int)e.code, s, e.message);
  assert(e.code == code && e.line && e.column && *e.message);
  assert(e.span.offset <= strlen(s) && e.span.length <= strlen(s) - e.span.offset);
}
static void test_parse(void) {
  mulib_yantlr_module_t *m;
  mulib_yantlr_run_t *r;
  mulib_yantlr_error_t e;
  mulib_yantlr_span_t span;
  char owned[] = "/* comment */ intermediary \"T\"; // tail\nset A {\"x\\u0000y\": [true,false,null,-1.25e2,\"\\uD83D\\uDE00\"]};";
  const mulib_yantlr_value_t *a;
  size_t n;
  m = parse(owned); memset(owned, '!', sizeof owned - 1);
  assert(!strcmp(mulib_yantlr_name(m), "T"));
  assert(mulib_yantlr_declaration_count(m) == 1);
  assert(!strcmp(mulib_yantlr_declaration(m, 0, &span), "A") && span.length);
  assert(!mulib_yantlr_declaration(m, 1, &span));
  assert(mulib_yantlr_dependency_count(m) == 0 && !mulib_yantlr_dependency(m, 0));
  r = mulib_yantlr_evaluate(m, NULL, 0, NULL, &e); assert(r);
  a = mulib_yantlr_get(r, "A");
  assert(mulib_yantlr_size(a) == 1 && !mulib_yantlr_member(a, "x"));
  assert(mulib_yantlr_key(a, 0, &n) && n == 3);
  a = mulib_yantlr_at(a, 0);
  assert(mulib_yantlr_boolean(mulib_yantlr_at(a, 0)));
  assert(!mulib_yantlr_boolean(mulib_yantlr_at(a, 1)));
  assert(mulib_yantlr_typeof(mulib_yantlr_at(a, 2)) == MULIB_YANTLR_NULL);
  assert(mulib_yantlr_numeric(mulib_yantlr_at(a, 3)) == -125);
  assert(mulib_yantlr_text(mulib_yantlr_at(a, 4), &n) && n == 4);
  mulib_yantlr_run_destroy(r); mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"Empty\";");
  r = mulib_yantlr_evaluate(m, NULL, 0, NULL, &e); assert(r);
  assert(!mulib_yantlr_get(r, "missing"));
  mulib_yantlr_run_destroy(r); mulib_yantlr_module_destroy(m);
  malformed("", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"\";", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\\u0000\";", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; trailing", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; /* unterminated", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A 01;", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A 1e999;", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A \"\\q\";", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A \"\\ud800\";", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A \"\xff\";", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A [1,];", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A {\"a\":1,};", MULIB_YANTLR_SYNTAX);
  malformed("intermediary \"x\"; set A 1; set A 2;", MULIB_YANTLR_DUPLICATE);
  malformed("intermediary \"x\"; set A {\"a\":1,\"\\u0061\":2};", MULIB_YANTLR_DUPLICATE);
  malformed("intermediary \"x\"; set A \\x x -> x;", MULIB_YANTLR_DUPLICATE);
  malformed("intermediary \"x\"; set true 1;", MULIB_YANTLR_SYNTAX);
  malformed("intermediaryx \"x\";", MULIB_YANTLR_SYNTAX);
  assert(!mulib_yantlr_parse(NULL, 1, NULL, &e) && e.code == MULIB_YANTLR_INVALID);
  assert(!mulib_yantlr_load("missing-file.y1", NULL, &e) && e.code == MULIB_YANTLR_IO);
}
static void test_evaluation(void) {
  const char *text = "intermediary \"Core\";\n"
    "set Filter \\l -> l.code `geq` Services[Diag]::Levels::Warning!Numeric;\n"
    "set Nested (\\x -> \\y -> x `add` y) 40 2;\n"
    "set Local (\\x y -> x `add` y) 20 22;\n"
    "set Partial add 40; set Answer Partial 2;\n"
    "set Forward Later; set Later Gen [ Language ] :: Identity ! {\"n\": Nested};\n"
    "set Selected Forward.n; set Built Gen[Language]::Generate!{};";
  mulib_yantlr_module_t *m = parse(text);
  mulib_yantlr_binding_t b[] = {
    {"geq", 2, geq, NULL, NULL}, {"add", 2, add, NULL, NULL},
    {"Services[Diag]::Levels::Warning", 1, identity, NULL, NULL},
    {"Numeric", 0, NULL, NULL, NULL},
    {"Gen[Language]::Identity", 1, identity, NULL, NULL},
    {"Gen[Language]::Generate", 1, generate, NULL, NULL}
  };
  mulib_yantlr_error_t e;
  mulib_yantlr_run_t *r;
  const mulib_yantlr_value_t *arg, *record, *answer;
  const mulib_yantlr_artifact_t *artifact;
  const char *key = "code";
  mulib_json_t *warning = mulib_json_number(5);
  b[3].constant = warning;
  assert(mulib_yantlr_dependency_count(m) == 6);
  assert(!strcmp(mulib_yantlr_dependency(m, 0)->name, "geq"));
  calls = 0; destroyed = 0;
  r = mulib_yantlr_evaluate(m, b, sizeof b / sizeof *b, NULL, &e);
  assert(r && e.code == MULIB_YANTLR_OK && calls == 1);
  mulib_json_destroy(warning); memset(b, 0, sizeof b);
  assert(mulib_yantlr_numeric(mulib_yantlr_get(r, "Nested")) == 42);
  assert(mulib_yantlr_numeric(mulib_yantlr_get(r, "Local")) == 42);
  assert(mulib_yantlr_numeric(mulib_yantlr_get(r, "Answer")) == 42);
  assert(mulib_yantlr_numeric(mulib_yantlr_get(r, "Selected")) == 42);
  arg = mulib_yantlr_number(r, 8); record = mulib_yantlr_record(r, &key, &arg, 1);
  answer = mulib_yantlr_apply(r, mulib_yantlr_get(r, "Filter"), record);
  assert(answer && mulib_yantlr_boolean(answer) && calls == 2);
  assert(mulib_yantlr_artifact_count(r) == 2);
  artifact = mulib_yantlr_artifact(r, 0);
  assert(artifact && !strcmp(artifact->path, "generated/main.c") && artifact->length == 29);
  assert(mulib_yantlr_artifact(r, 1)->length == 0 && !mulib_yantlr_artifact(r, 2));
  assert(mulib_yantlr_pointer(mulib_yantlr_get(r, "Built"), "test.generator"));
  assert(!mulib_yantlr_pointer(mulib_yantlr_get(r, "Built"), "wrong.type"));
  mulib_yantlr_run_destroy(r); assert(destroyed == 1);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"Order\"; set F \\x -> Later; set Independent Make!2; set Later Make!1;");
  b[0].name = "Make"; b[0].arity = 1; b[0].function = identity;
  calls = 0;
  r = mulib_yantlr_evaluate(m, b, 1, NULL, &e); assert(r);
  assert(calls == 2 && sequence[0] == 1 && sequence[1] == 2);
  mulib_yantlr_run_destroy(r); mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"Scope\"; set F (\\x -> (\\x -> x) 2) 1; set G (\\add -> add) 42;");
  assert(mulib_yantlr_dependency_count(m) == 0);
  r = mulib_yantlr_evaluate(m, NULL, 0, NULL, &e); assert(r);
  assert(mulib_yantlr_numeric(mulib_yantlr_get(r, "F")) == 2);
  assert(mulib_yantlr_numeric(mulib_yantlr_get(r, "G")) == 42);
  mulib_yantlr_run_destroy(r); mulib_yantlr_module_destroy(m);
}
static void test_failures(void) {
  mulib_yantlr_binding_t b[] = {{"Make", 1, identity, NULL, NULL}};
  mulib_yantlr_module_t *m;
  mulib_yantlr_run_t *r;
  mulib_yantlr_error_t e;
  mulib_yantlr_options_t o;
  const char *bad[] = {"../escape.c", "/absolute", "a//b", "a/./b", "a/../b", "a\\b", "C:x", "a/", "\n"};
  size_t i;
  calls = 0;
  m = parse("intermediary \"x\"; set First Make!1;\nset Unknown Services[Missing]::Foo!{};");
  assert(!mulib_yantlr_evaluate(m, b, 1, NULL, &e));
  assert(e.code == MULIB_YANTLR_SERVICE && e.line == 2 && strstr(e.message, "Services[Missing]::Foo") && calls == 0);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set A Make!1;");
  {
    mulib_yantlr_binding_t duplicate[] = {{"Make", 1, identity, NULL, NULL}, {"Make", 1, identity, NULL, NULL}};
    calls = 0;
    assert(!mulib_yantlr_evaluate(m, duplicate, 2, NULL, &e) && e.code == MULIB_YANTLR_DUPLICATE && calls == 0);
    duplicate[1].name = "A";
    assert(!mulib_yantlr_evaluate(m, duplicate, 2, NULL, &e) && e.code == MULIB_YANTLR_DUPLICATE && calls == 0);
  }
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set First Make!1; set Cycle Other; set Other Cycle;");
  assert(!mulib_yantlr_evaluate(m, b, 1, NULL, &e) && e.code == MULIB_YANTLR_CYCLE && calls == 0);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set F \\x -> Missing;");
  assert(!mulib_yantlr_evaluate(m, NULL, 0, NULL, &e) && e.code == MULIB_YANTLR_NAME);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set Omega (\\x -> x x) (\\x -> x x);");
  assert(!mulib_yantlr_evaluate(m, NULL, 0, NULL, &e) && e.code == MULIB_YANTLR_LIMIT);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set A 1!2;");
  assert(!mulib_yantlr_evaluate(m, NULL, 0, NULL, &e) && e.code == MULIB_YANTLR_TYPE);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set A {}.missing;");
  assert(!mulib_yantlr_evaluate(m, NULL, 0, NULL, &e) && e.code == MULIB_YANTLR_TYPE);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set A Make!{};");
  b[0].function = broken;
  assert(!mulib_yantlr_evaluate(m, b, 1, NULL, &e) && e.code == MULIB_YANTLR_PROVIDER);
  assert(!strcmp(e.message, "intentional failure"));
  b[0].function = generate; destroyed = 0;
  memset(&o, 0, sizeof o); o.max_artifact_bytes = 1;
  assert(!mulib_yantlr_evaluate(m, b, 1, &o, &e) && e.code == MULIB_YANTLR_LIMIT && destroyed == 1);
  b[0].arity = 0;
  assert(!mulib_yantlr_evaluate(m, b, 1, NULL, &e) && e.code == MULIB_YANTLR_INVALID);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set X 1;");
  memset(&o, 0, sizeof o); o.max_steps = 1;
  assert(!mulib_yantlr_evaluate(m, NULL, 0, &o, &e) && e.code == MULIB_YANTLR_LIMIT);
  memset(&o, 0, sizeof o); o.parser.max_steps = 1;
  assert(!mulib_yantlr_parse("intermediary \"x\";", 17, &o, &e) && e.code == MULIB_YANTLR_LIMIT);
  memset(&o, 0, sizeof o); o.max_depth = 1;
  assert(!mulib_yantlr_evaluate(m, NULL, 0, &o, &e) && e.code == MULIB_YANTLR_LIMIT);
  mulib_yantlr_module_destroy(m);
  m = parse("intermediary \"x\"; set X [1,2,3];");
  memset(&o, 0, sizeof o); o.max_values = 1;
  assert(!mulib_yantlr_evaluate(m, NULL, 0, &o, &e) && e.code == MULIB_YANTLR_LIMIT);
  mulib_yantlr_module_destroy(m);
  for (i = 0; i < sizeof bad / sizeof *bad; ++i) {
    m = parse("intermediary \"x\";");
    r = mulib_yantlr_evaluate(m, NULL, 0, NULL, &e); assert(r);
    assert(!mulib_yantlr_emit(r, bad[i], "", 0));
    assert(mulib_yantlr_error(r)->code == MULIB_YANTLR_OUTPUT && !mulib_yantlr_artifact_count(r));
    mulib_yantlr_run_destroy(r); mulib_yantlr_module_destroy(m);
  }
  m = parse("intermediary \"x\";");
  r = mulib_yantlr_evaluate(m, NULL, 0, NULL, &e); assert(r);
  assert(mulib_yantlr_emit(r, "file.c", "abc", 3));
  assert(!mulib_yantlr_emit(r, "file.c", "abc", 3));
  assert(!mulib_yantlr_artifact_count(r) && !mulib_yantlr_artifact(r, 0));
  mulib_yantlr_run_destroy(r); mulib_yantlr_module_destroy(m);
}
static void test_specimens(void) {
  const char *files[] = {"Lua.y1", "YBASIC.y1"};
  size_t i;
  for (i = 0; i < 2; ++i) {
    char path[4096];
    const char *file = __FILE__, *slash = strrchr(file, '/');
    mulib_yantlr_error_t e;
    mulib_yantlr_module_t *m;
    mulib_yantlr_run_t *r;
    /* Both CMake and tools/check.py compile this file using an absolute path. */
    assert(slash);
    snprintf(path, sizeof path, "%.*s/../examples/%s", (int)(slash - file), file, files[i]);
    m = mulib_yantlr_load(path, NULL, &e);
    if (!m) fprintf(stderr, "%s:%zu:%zu %s\n", path, e.line, e.column, e.message);
    assert(m && mulib_yantlr_declaration_count(m) > 10 && mulib_yantlr_dependency_count(m) > 10);
    r = mulib_yantlr_evaluate(m, NULL, 0, NULL, &e);
    assert(!r && e.code == (i ? MULIB_YANTLR_NAME : MULIB_YANTLR_SERVICE));
    {
      size_t count = mulib_yantlr_dependency_count(m), j;
      mulib_yantlr_binding_t *bindings = (mulib_yantlr_binding_t *)calloc(count, sizeof *bindings);
      mulib_json_t *numeric = mulib_json_number(1);
      assert(bindings && numeric);
      /* Identity mocks test the complete host evaluator, NOT the proposed
       * language services or their declarative mini-languages. */
      for (j = 0; j < count; ++j) {
        bindings[j].name = mulib_yantlr_dependency(m, j)->name;
        bindings[j].arity = 1; bindings[j].function = identity;
        if (!strcmp(bindings[j].name, "geq")) { bindings[j].arity = 2; bindings[j].function = geq; }
        if (!strcmp(bindings[j].name, "Numeric")) { bindings[j].function = NULL; bindings[j].constant = numeric; }
      }
      r = mulib_yantlr_evaluate(m, bindings, count, NULL, &e); assert(r);
      assert(mulib_yantlr_typeof(mulib_yantlr_get(r, "Build")) == MULIB_YANTLR_RECORD);
      assert(mulib_yantlr_artifact_count(r) == 0);
      mulib_yantlr_run_destroy(r); mulib_json_destroy(numeric); free(bindings);
    }
    mulib_yantlr_module_destroy(m);
  }
}
int main(void) {
  test_parse(); test_evaluation(); test_failures(); test_specimens();
  puts("yantlr core tests passed"); return 0;
}
