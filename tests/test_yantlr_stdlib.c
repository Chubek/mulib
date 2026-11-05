#define MULIB_IMPL_ARENA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_YANTLR
#define MULIB_IMPL_YANTLR_STDLIB
#include "../library/yantlr/mulib-yantlr-stdlib.h"
#include <assert.h>

static mulib_yantlr_stdlib_t *library;
static mulib_yantlr_module_t *module;
static mulib_yantlr_error_t error;
static mulib_yantlr_run_t *evaluate(const char *body) {
  mulib_strbuf_t source;
  const mulib_yantlr_binding_t *bindings;
  size_t count;
  mulib_strbuf_init(&source);
  assert(mulib_strbuf_append(&source, "intermediary \"test\"; "));
  assert(mulib_strbuf_append(&source, body));
  module = mulib_yantlr_parse(source.data, source.length, NULL, &error);
  if (!module) fprintf(stderr, "parse: %s\n", error.message);
  assert(module); mulib_strbuf_free(&source);
  bindings = mulib_yantlr_stdlib_bindings(library, &count);
  return mulib_yantlr_evaluate(module, bindings, count, NULL, &error);
}
static void done(mulib_yantlr_run_t *r) {
  mulib_yantlr_run_destroy(r); mulib_yantlr_module_destroy(module);
}
static void fails(const char *body, mulib_yantlr_error_code_t code) {
  mulib_yantlr_run_t *r = evaluate(body);
  if (r || error.code != code) fprintf(stderr, "%s: expected %d got %d (%s)\n", body, (int)code, (int)error.code, error.message);
  assert(!r && error.code == code && *error.message); done(r);
}
static void number(mulib_yantlr_run_t *r, const char *name, double expected) {
  const mulib_yantlr_value_t *v = mulib_yantlr_get(r, name);
  assert(v && mulib_yantlr_typeof(v) == MULIB_YANTLR_NUMBER && mulib_yantlr_numeric(v) == expected);
}
static void text(mulib_yantlr_run_t *r, const char *name, const char *expected, size_t n) {
  size_t length;
  const char *s = mulib_yantlr_text(mulib_yantlr_get(r, name), &length);
  assert(s && length == n && !memcmp(s, expected, n));
}
static void test_values(void) {
  mulib_yantlr_run_t *r = evaluate(
    "set A add 40 2; set B sub 43 1; set C mul 6 7; set D div 84 2; set E mod -7 3;"
    "set F Services[Array]::Fold add 0 (Services[Array]::Map (\\x -> x `mul` x) (Services[Array]::Range!5));"
    "set G Services[Array]::Filter (\\x -> x `geq` 2) ([0,1,2,3]);"
    "set H Services[Array]::At G 1; set I Services[Array]::Concat G ([4]);"
    "set J Services[Array]::Length I; set K Services[Array]::Map identity ([]);"
    "set L Services[Array]::Fold add 42 ([]);"
    "set M Services[String]::Concat \"a\\u0000\" \"b\";"
    "set N Services[String]::Slice M 1 2; set O Services[String]::Length M;"
    "set P Services[String]::Join \",\" ([\"a\",\"b\",\"\"]);"
    "set Q Services[String]::Join \",\" ([]);"
    "set R Services[Record]::Get {\"a\\u0000b\":42,\"nil\":null} \"a\\u0000b\";"
    "set S Services[Record]::Has {\"nil\":null} \"nil\";"
    "set T Services[Record]::Has {} \"missing\";"
    "set U Services[Record]::Keys {\"a\\u0000b\":1}; set V Services[Record]::Values {\"a\":42};"
    "set W Services[Core]::Choose true 42 0; set X Services[Core]::Type V;"
    "set Y and (not false) (or true false);"
    "set Z eq null null; set AA neq true false; set AB lt \"a\\u0000\" \"a\\u0001\";"
    "set AC leq 1 1; set AD gt 2 1; set AE identity 42;");
  assert(r);
  number(r, "A", 42); number(r, "B", 42); number(r, "C", 42); number(r, "D", 42); number(r, "E", -1);
  number(r, "F", 30); number(r, "H", 3); number(r, "J", 3); number(r, "L", 42);
  number(r, "O", 3); number(r, "R", 42); number(r, "W", 42); number(r, "AE", 42);
  assert(mulib_yantlr_size(mulib_yantlr_get(r, "K")) == 0);
  text(r, "M", "a\0b", 3); text(r, "N", "\0b", 2); text(r, "P", "a,b,", 4); text(r, "Q", "", 0); text(r, "X", "array", 5);
  assert(mulib_yantlr_boolean(mulib_yantlr_get(r, "S")) && !mulib_yantlr_boolean(mulib_yantlr_get(r, "T")));
  assert(mulib_yantlr_size(mulib_yantlr_get(r, "U")) == 1);
  assert(mulib_yantlr_numeric(mulib_yantlr_at(mulib_yantlr_get(r, "V"), 0)) == 42);
  assert(mulib_yantlr_boolean(mulib_yantlr_get(r, "Y")) && mulib_yantlr_boolean(mulib_yantlr_get(r, "Z")));
  assert(mulib_yantlr_boolean(mulib_yantlr_get(r, "AA")) && mulib_yantlr_boolean(mulib_yantlr_get(r, "AB")));
  assert(mulib_yantlr_boolean(mulib_yantlr_get(r, "AC")) && mulib_yantlr_boolean(mulib_yantlr_get(r, "AD")));
  done(r);
}
static void test_services(void) {
  const mulib_yantlr_value_t *v;
  mulib_yantlr_run_t *r = evaluate(
    "set Encoded Services[JSON]::Stringify!{\"a\\u0000b\":[null,true,42,\"x\\u0000y\"]};"
    "set Decoded Services[JSON]::Parse Encoded;"
    "set Levels [Services[Diag]::Levels::Debug!Numeric, Services[Diag]::Levels::Info!Numeric,"
    "Services[Diag]::Levels::Warning!Numeric, Services[Diag]::Levels::Error!Numeric];"
    "set Filter \\l -> l.code `geq` Services[Diag]::Levels::Warning!Numeric;"
    "set Pass Services[Diag]::Assert true \"fine\";"
    "set Grammar Services[Parse]::PEG!\"document <- word (' ' word)*; word <- [a-z]+;\";"
    "set Good Services[Parse]::Run Grammar \"hello world\";"
    "set Bad Services[Parse]::Run Grammar \"hello!\";"
    "set Empty Services[Parse]::Run (Services[Parse]::EBNF!\"document = ;\") \"\";"
    "set Build Services[IO]::Emit \"report.json\" Encoded;"
    "set Zero Services[IO]::Emit \"empty\" \"\";");
  size_t n, i;
  assert(r);
  v = mulib_yantlr_get(r, "Decoded"); assert(mulib_yantlr_key(v, 0, &n) && n == 3);
  v = mulib_yantlr_at(v, 0); assert(mulib_yantlr_size(v) == 4);
  assert(mulib_yantlr_text(mulib_yantlr_at(v, 3), &n) && n == 3);
  for (i = 0; i < 4; ++i) assert(mulib_yantlr_numeric(mulib_yantlr_at(mulib_yantlr_get(r, "Levels"), i)) == (double)i);
  assert(mulib_yantlr_boolean(mulib_yantlr_member(mulib_yantlr_get(r, "Good"), "matched")));
  assert(!mulib_yantlr_boolean(mulib_yantlr_member(mulib_yantlr_get(r, "Bad"), "matched")));
  assert(mulib_yantlr_numeric(mulib_yantlr_member(mulib_yantlr_get(r, "Bad"), "offset")) == 5);
  v = mulib_yantlr_member(mulib_yantlr_get(r, "Good"), "captures"); assert(mulib_yantlr_size(v) == 3);
  assert(mulib_yantlr_numeric(mulib_yantlr_member(mulib_yantlr_at(v, 1), "offset")) == 6);
  assert(mulib_yantlr_boolean(mulib_yantlr_member(mulib_yantlr_get(r, "Empty"), "matched")));
  assert(mulib_yantlr_artifact_count(r) == 2 && mulib_yantlr_artifact(r, 1)->length == 0);
  /* Registry copies and closures remain callable after evaluate returns. */
  v = mulib_yantlr_number(r, 2);
  { const char *key = "code"; v = mulib_yantlr_record(r, &key, &v, 1); }
  assert(mulib_yantlr_boolean(mulib_yantlr_apply(r, mulib_yantlr_get(r, "Filter"), v)));
  done(r);
}
static void test_failures(void) {
  fails("set A add true 2;", MULIB_YANTLR_TYPE);
  fails("set A div 1 0;", MULIB_YANTLR_TYPE);
  fails("set A mod 1 -0;", MULIB_YANTLR_TYPE);
  fails("set A mul 1e308 1e308;", MULIB_YANTLR_TYPE);
  fails("set A eq [] [];", MULIB_YANTLR_TYPE);
  fails("set A eq 1 true;", MULIB_YANTLR_TYPE);
  fails("set A lt false true;", MULIB_YANTLR_TYPE);
  fails("set A and true 1;", MULIB_YANTLR_TYPE);
  fails("set A Services[Core]::Choose 1 2 3;", MULIB_YANTLR_TYPE);
  fails("set A Services[Array]::At ([1]) 1;", MULIB_YANTLR_TYPE);
  fails("set A Services[Array]::At ([]) -1;", MULIB_YANTLR_TYPE);
  fails("set A Services[Array]::Range!1.5;", MULIB_YANTLR_TYPE);
  fails("set A Services[Array]::Range!18446744073709551616;", MULIB_YANTLR_TYPE);
  fails("set A Services[Array]::Filter identity ([1]);", MULIB_YANTLR_TYPE);
  fails("set A Services[Array]::Map (\\x -> div x 0) ([1]);", MULIB_YANTLR_TYPE);
  fails("set A Services[String]::Slice \"é\" 1 1;", MULIB_YANTLR_TYPE);
  fails("set A Services[String]::Slice \"abc\" 2 2;", MULIB_YANTLR_TYPE);
  fails("set A Services[String]::Join \",\" ([1]);", MULIB_YANTLR_TYPE);
  fails("set A Services[Record]::Get {} \"x\";", MULIB_YANTLR_NAME);
  fails("set A Services[JSON]::Parse!\"{\";", MULIB_YANTLR_SYNTAX);
  fails("set A Services[JSON]::Parse!\"1e999\";", MULIB_YANTLR_SYNTAX);
  fails("set A Services[JSON]::Stringify identity;", MULIB_YANTLR_TYPE);
  fails("set A Services[Diag]::Assert false \"failed\";", MULIB_YANTLR_PROVIDER);
  assert(!strcmp(error.message, "failed"));
  fails("set A Services[Diag]::Fail!\"bad\\u0000tail\";", MULIB_YANTLR_TYPE);
  fails("set A Services[Diag]::Levels::Warning!\"unknown\";", MULIB_YANTLR_TYPE);
  fails("set A Services[IO]::Emit \"../bad\" \"x\";", MULIB_YANTLR_OUTPUT);
  fails("set A Services[IO]::Emit \"ok\\u0000bad\" \"x\";", MULIB_YANTLR_OUTPUT);
  fails("set A Services[IO]::Emit \"same\" \"x\"; set B Services[IO]::Emit \"same\" \"y\";", MULIB_YANTLR_OUTPUT);
  fails("set A Services[Parse]::PEG!\"x <- x;\";", MULIB_YANTLR_SYNTAX);
  fails("set A Services[Parse]::Run 1 \"x\";", MULIB_YANTLR_TYPE);
  fails("set A Gen[Language]::Parser!{};", MULIB_YANTLR_SERVICE);
}
static void test_limits(void) {
  mulib_yantlr_stdlib_options_t o = {0};
  mulib_yantlr_stdlib_destroy(library);
  o.max_items = 3; o.max_bytes = 8; o.max_depth = 2;
  library = mulib_yantlr_stdlib_create(&o); assert(library);
  fails("set A Services[Array]::Range!4;", MULIB_YANTLR_LIMIT);
  fails("set A Services[Array]::Concat ([1,2]) ([3,4]);", MULIB_YANTLR_LIMIT);
  fails("set A Services[String]::Concat \"12345\" \"6789\";", MULIB_YANTLR_LIMIT);
  fails("set A Services[JSON]::Stringify![[1]];", MULIB_YANTLR_LIMIT);
  fails("set A Services[JSON]::Parse!\"[[1]]\";", MULIB_YANTLR_LIMIT);
  fails("set A Services[JSON]::Stringify!\"\\n\\n\\n\\n\";", MULIB_YANTLR_LIMIT);
  fails("set A Services[Array]::Map identity ([1,2,3,4]);", MULIB_YANTLR_LIMIT);
  fails("set A Services[Array]::Fold add 0 ([1,2,3,4]);", MULIB_YANTLR_LIMIT);
  fails("set A Services[Record]::Has {\"a\":1,\"b\":2,\"c\":3,\"d\":4} \"e\";", MULIB_YANTLR_LIMIT);
  fails("set A Services[String]::Join \",\" ([\"a\",\"b\",\"c\",\"d\"]);", MULIB_YANTLR_LIMIT);
  {
    mulib_yantlr_run_t *r = evaluate("set A Services[String]::Concat \"1234\" \"5678\";"
        "set B Services[Array]::Range!3;");
    assert(r); text(r, "A", "12345678", 8);
    assert(mulib_yantlr_size(mulib_yantlr_get(r, "B")) == 3); done(r);
  }
  mulib_yantlr_stdlib_destroy(library);
  memset(&o, 0, sizeof o); o.parser.max_steps = 1;
  library = mulib_yantlr_stdlib_create(&o); assert(library);
  fails("set A Services[Parse]::Run (Services[Parse]::PEG!\"x <- 'a';\") \"a\";", MULIB_YANTLR_LIMIT);
}
static void test_example(void) {
  char path[4096];
  const char *file = __FILE__, *slash = strrchr(file, '/');
  const mulib_yantlr_binding_t *bindings;
  size_t count;
  mulib_yantlr_run_t *r;
  assert(slash);
  snprintf(path, sizeof path, "%.*s/../library/yantlr/example.y1", (int)(slash - file), file);
  module = mulib_yantlr_load(path, NULL, &error); assert(module);
  bindings = mulib_yantlr_stdlib_bindings(library, &count);
  r = mulib_yantlr_evaluate(module, bindings, count, NULL, &error);
  if (!r) fprintf(stderr, "example: %s\n", error.message);
  assert(r && mulib_yantlr_artifact_count(r) == 1); number(r, "Total", 29); done(r);
}
int main(void) {
  size_t count = 1;
  assert(!mulib_yantlr_stdlib_bindings(NULL, &count) && !count);
  mulib_yantlr_stdlib_destroy(NULL);
  library = mulib_yantlr_stdlib_create(NULL); assert(library);
  test_values(); test_services(); test_failures(); test_example(); test_limits();
  mulib_yantlr_stdlib_destroy(library);
  puts("yantlr standard-library tests passed"); return 0;
}
