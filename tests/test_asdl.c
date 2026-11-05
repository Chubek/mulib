#define MULIB_IMPL_ASDL
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#include "mulib-asdl.h"
/* These dependencies are only for checking the emitted formats. */
#define MULIB_IMPL_SEXPR
#define MULIB_IMPL_JSON
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PEG
#define MULIB_IMPL_YANTLR
#include "mulib-sexpr.h"
#include "mulib-yantlr.h"
#include <assert.h>

static const char source[] =
  "-- Products, sums, unnamed fields, recursion and shared attributes.\n"
  "module Trees {\n"
  "  pos = (string? file, int line, int offset)\n"
  "  stm = Compound(stm head, stm next)\n"
  "      | Assign(identifier lval, exp rval) | Print(exp* args)\n"
  "      attributes (pos p)\n"
  "  exp = Id(identifier) | Num(int value) | Op(exp, binop, exp)\n"
  "      | Empty attributes (int? line)\n"
  "  binop = Plus | Minus | Times | Div\n"
  "  keyword = (int switch, string _bad)\n";
/* The generator fixture exercises C keywords and injective underscore mapping. */
static const char fixture[] =
  "module Trees {\n"
  "pos = (string? file, int line, int offset)\n"
  "stm = Compound(stm head, stm next) | Assign(identifier lval, exp rval)\n"
  "    | Print(exp* args) attributes (pos p)\n"
  "exp = Id(identifier) | Num(int value) | Op(exp, binop, exp) | Empty\n"
  "    attributes (int? line)\n"
  "binop = Plus | Minus | Times | Div\n"
  "keyword = (int switch, string inline, int, int int1, binop? operator, int* values)\n"
  "node_name = With_under(keyword while) | Withunder attributes (string? File)\n"
  "node_uname = Other\n"
  "}\n";
static mulib_asdl_module_t *parse(const char *s) {
  mulib_asdl_error_t e;
  mulib_asdl_module_t *m = mulib_asdl_parse(s, strlen(s), NULL, &e);
  if (!m) fprintf(stderr, "%zu:%zu: %s\n", e.line, e.column, e.message);
  assert(m && e.code == MULIB_ASDL_OK); return m;
}
static void malformed(const char *s, mulib_asdl_error_code_t code) {
  mulib_asdl_error_t e;
  assert(!mulib_asdl_parse(s, strlen(s), NULL, &e));
  if (e.code != code) fprintf(stderr, "input: %s\nexpected %d got %d: %s\n", s, (int)code, (int)e.code, e.message);
  assert(e.code == code && e.line && e.column && e.message[0]);
  assert(e.span.offset <= strlen(s) && e.span.length <= strlen(s) - e.span.offset);
}
static void test_parser(void) {
  mulib_asdl_module_t *m;
  const mulib_asdl_type_t *t;
  const mulib_asdl_field_t *f;
  char owned[sizeof fixture];
  memcpy(owned, fixture, sizeof fixture); m = parse(owned);
  memset(owned, '!', sizeof owned);
  assert(!strcmp(mulib_asdl_name(m), "Trees") && mulib_asdl_type_count(m) == 7);
  assert(!mulib_asdl_type(m, 7) && !mulib_asdl_find(m, "absent"));
  t = mulib_asdl_find(m, "pos"); assert(t && t->kind == MULIB_ASDL_PRODUCT && t->field_count == 3);
  f = &t->fields[0];
  assert(!strcmp(f->type, "string") && !strcmp(f->name, "file") && f->cardinality == MULIB_ASDL_OPTIONAL);
  assert(f->span.offset && f->span.length && !mulib_asdl_is_enum(t));
  t = mulib_asdl_find(m, "exp"); assert(t && t->constructor_count == 4 && t->attribute_count == 1);
  assert(!t->constructors[0].fields[0].name && !t->constructors[2].fields[2].name);
  assert(t->attributes[0].cardinality == MULIB_ASDL_OPTIONAL && !mulib_asdl_is_enum(t));
  assert(mulib_asdl_is_enum(mulib_asdl_find(m, "binop")));
  mulib_asdl_destroy(m);
  m = parse(""); assert(!mulib_asdl_type_count(m)); mulib_asdl_destroy(m);
  m = parse("module Empty {} -- eof"); assert(!mulib_asdl_type_count(m)); mulib_asdl_destroy(m);
  m = parse(" /* a\n b */ t = A // x\n | B -- y\n"); mulib_asdl_destroy(m);
  m = parse("a = (b x) b = C(a? x)"); mulib_asdl_destroy(m);
  m = parse("t = A(t* children, t? next)"); mulib_asdl_destroy(m);
  m = parse("t = A(int Attributes, int module)"); mulib_asdl_destroy(m);
  malformed(source, MULIB_ASDL_SYNTAX);
  malformed("T = A", MULIB_ASDL_SYNTAX);
  malformed("_t = A", MULIB_ASDL_SYNTAX);
  malformed("t = a", MULIB_ASDL_SYNTAX);
  malformed("t = A()", MULIB_ASDL_SYNTAX);
  malformed("t = ()", MULIB_ASDL_SYNTAX);
  malformed("t = A(int,)", MULIB_ASDL_SYNTAX);
  malformed("t = A(int**)", MULIB_ASDL_SYNTAX);
  malformed("t = A((int,int))", MULIB_ASDL_SYNTAX);
  malformed("t = A |", MULIB_ASDL_SYNTAX);
  malformed("t = A;", MULIB_ASDL_SYNTAX);
  malformed("t = (int) attributes (int x)", MULIB_ASDL_SYNTAX);
  malformed("t = A attributes ()", MULIB_ASDL_SYNTAX);
  malformed("t = A attributesX (int)", MULIB_ASDL_SYNTAX);
  malformed("module X { t = A", MULIB_ASDL_SYNTAX);
  malformed("module X {} garbage", MULIB_ASDL_SYNTAX);
  malformed("t = A /* unfinished", MULIB_ASDL_SYNTAX);
  malformed("t = A\n t = B", MULIB_ASDL_DUPLICATE);
  malformed("t = A | A", MULIB_ASDL_DUPLICATE);
  malformed("t = A u = A", MULIB_ASDL_DUPLICATE);
  malformed("t = A(int x, string x)", MULIB_ASDL_DUPLICATE);
  malformed("t = A(int x) attributes (string x)", MULIB_ASDL_DUPLICATE);
  malformed("t = A attributes (int x, int x)", MULIB_ASDL_DUPLICATE);
  malformed("int = A", MULIB_ASDL_NAME);
  malformed("string = (int)", MULIB_ASDL_NAME);
  malformed("identifier = A", MULIB_ASDL_NAME);
  malformed("attributes = A", MULIB_ASDL_NAME);
  malformed("t = A(missing? x)", MULIB_ASDL_NAME);
  malformed("t = A attributes (missing* xs)", MULIB_ASDL_NAME);
  malformed("t = (t)", MULIB_ASDL_CYCLE);
  malformed("a = (b?) b = (a*)", MULIB_ASDL_CYCLE);
  assert(!mulib_asdl_name(NULL) && !mulib_asdl_type_count(NULL) && !mulib_asdl_type(NULL, 0));
  assert(!mulib_asdl_find(NULL, "x") && !mulib_asdl_is_enum(NULL));
  mulib_asdl_destroy(NULL);
}
static void test_api(void) {
  static const mulib_asdl_field_t pair[] = {MULIB_ASDL_FIELD("int", "left"), MULIB_ASDL_OPT("string", NULL)};
  static const mulib_asdl_field_t items[] = {MULIB_ASDL_SEQ("pair", "items")};
  static const mulib_asdl_field_t attrs[] = {MULIB_ASDL_FIELD("int", "line")};
  static const mulib_asdl_constructor_t cs[] = {MULIB_ASDL_ATOM("Empty"), MULIB_ASDL_CONSTRUCTOR("Many", items)};
  static const mulib_asdl_constructor_t enums[] = {MULIB_ASDL_ATOM("Yes"), MULIB_ASDL_ATOM("No")};
  static const mulib_asdl_type_t types[] = {
    MULIB_ASDL_PRODUCT_TYPE("pair", pair), MULIB_ASDL_ATTR_SUM("tree", cs, attrs), MULIB_ASDL_SUM_TYPE("answer", enums)
  };
  mulib_asdl_error_t e;
  mulib_asdl_module_t *a = mulib_asdl_from_types("Test", types, MULIB_ASDL_COUNT(types), &e);
  mulib_asdl_module_t *b = parse("module Test { pair = (int left, string?) tree = Empty | Many(pair* items) attributes (int line) answer = Yes | No }");
  mulib_strbuf_t x = {0}, y = {0};
  mulib_asdl_field_t field = mulib_asdl_field("int", NULL, MULIB_ASDL_REQUIRED);
  mulib_asdl_constructor_t ctor = mulib_asdl_constructor("C", &field, 1);
  mulib_asdl_type_t type = mulib_asdl_sum("t", &ctor, 1, NULL, 0);
  assert(a && e.code == MULIB_ASDL_OK);
  assert(mulib_asdl_generate(a, NULL, &x, &e) && mulib_asdl_generate(b, NULL, &y, &e));
  assert(x.length == y.length && !strcmp(x.data, y.data));
  mulib_asdl_destroy(a); mulib_asdl_destroy(b); mulib_strbuf_free(&x); mulib_strbuf_free(&y);
  a = mulib_asdl_from_types(NULL, &type, 1, &e); assert(a);
  memset(&field, 0, sizeof field); memset(&ctor, 0, sizeof ctor); memset(&type, 0, sizeof type);
  assert(!strcmp(mulib_asdl_type(a, 0)->constructors[0].fields[0].type, "int")); mulib_asdl_destroy(a);
  field = mulib_asdl_field("int", "class", MULIB_ASDL_REQUIRED);
  type = mulib_asdl_product("record", &field, 1);
  a = mulib_asdl_from_types(NULL, &type, 1, &e); assert(a); mulib_asdl_destroy(a);
  type.field_count = 0;
  assert(!mulib_asdl_from_types(NULL, &type, 1, &e) && e.code == MULIB_ASDL_INVALID);
  type.field_count = 1; type.fields = NULL;
  assert(!mulib_asdl_from_types(NULL, &type, 1, &e) && e.code == MULIB_ASDL_INVALID);
  type.fields = &field; field.cardinality = (mulib_asdl_cardinality_t)3;
  assert(!mulib_asdl_from_types(NULL, &type, 1, &e) && e.code == MULIB_ASDL_INVALID);
  field.cardinality = MULIB_ASDL_REQUIRED; field.name = "_invalid";
  assert(!mulib_asdl_from_types(NULL, &type, 1, &e) && e.code == MULIB_ASDL_NAME);
  assert(!mulib_asdl_from_types(NULL, NULL, 1, &e) && e.code == MULIB_ASDL_INVALID);
  assert(!mulib_asdl_from_types(NULL, &type, SIZE_MAX, &e) && e.code == MULIB_ASDL_LIMIT);
  a = mulib_asdl_from_types(NULL, NULL, 0, &e); assert(a); mulib_asdl_destroy(a);
}
static void test_formats(void) {
  mulib_asdl_module_t *m = parse(fixture);
  mulib_asdl_options_t o = {0};
  mulib_asdl_error_t e;
  mulib_strbuf_t b = {0}, again = {0};
  mulib_sexpr_t *tree;
  const char *end;
  mulib_yantlr_module_t *ym;
  mulib_yantlr_run_t *run;
  mulib_yantlr_error_t ye;
  const mulib_yantlr_value_t *schema, *types, *ct;
  assert(mulib_asdl_generate(m, NULL, &b, &e));
  tree = mulib_sexpr_parse(b.data, &end); assert(tree && *end == '\n' && !end[1]);
  assert(!strcmp(tree->child->atom, "asdl")); mulib_sexpr_destroy(tree);
  mulib_strbuf_free(&b); o.format = MULIB_ASDL_YANTLR;
  assert(mulib_asdl_generate(m, &o, &b, &e));
  assert(mulib_asdl_compile(fixture, sizeof fixture - 1, &o, &again, &e));
  assert(!strcmp(b.data, again.data)); mulib_strbuf_free(&again);
  ym = mulib_yantlr_parse(b.data, b.length, NULL, &ye);
  if (!ym) fprintf(stderr, "%zu:%zu: %s\n", ye.line, ye.column, ye.message);
  assert(ym && !mulib_yantlr_dependency_count(ym));
  run = mulib_yantlr_evaluate(ym, NULL, 0, NULL, &ye); assert(run);
  schema = mulib_yantlr_get(run, "Schema"); types = mulib_yantlr_member(schema, "types");
  assert(mulib_yantlr_size(types) == mulib_asdl_type_count(m));
  ct = mulib_yantlr_at(mulib_yantlr_member(mulib_yantlr_at(types, 2), "constructors"), 1);
  assert(mulib_yantlr_numeric(mulib_yantlr_member(ct, "tag")) == 2);
  assert(!strcmp(mulib_yantlr_text(mulib_yantlr_member(ct, "name"), NULL), "Num"));
  mulib_yantlr_run_destroy(run); mulib_yantlr_module_destroy(ym);
  mulib_strbuf_free(&b); o.format = MULIB_ASDL_C99; o.c_prefix = "test";
  assert(mulib_asdl_generate(m, &o, &b, &e));
  assert(strstr(b.data, "typedef int64_t test_t_int;") && strstr(b.data, "test_make_exp_Num"));
  assert(strstr(b.data, "test_t_node_uname") && strstr(b.data, "test_t_node_uuname"));
  mulib_strbuf_free(&b); mulib_asdl_destroy(m);
  assert(mulib_asdl_compile("t = A(int nil, int)", 19, NULL, &b, &e));
  assert(strstr(b.data, "\"nil\"") && strstr(b.data, "\"int\" ()"));
  mulib_strbuf_free(&b);
}
static void test_errors_limits(void) {
  mulib_asdl_error_t e;
  mulib_asdl_options_t o = {0};
  mulib_strbuf_t out = {0}, many = {0};
  mulib_asdl_module_t *m;
  size_t i;
  char longname[270];
  const char nul[] = {'t', '=', 'A', 0, 'x'};
  assert(!mulib_asdl_parse(NULL, 1, NULL, &e) && e.code == MULIB_ASDL_INVALID);
  assert(!mulib_asdl_parse("", SIZE_MAX, NULL, &e) && e.code == MULIB_ASDL_LIMIT);
  assert(!mulib_asdl_parse(nul, sizeof nul, NULL, &e) && e.code == MULIB_ASDL_SYNTAX);
  o.module_name = "Bare";
  m = mulib_asdl_parse(NULL, 0, &o, &e); assert(m && !strcmp(mulib_asdl_name(m), "Bare"));
  mulib_asdl_destroy(m); o.module_name = "_invalid";
  assert(!mulib_asdl_parse(NULL, 0, &o, &e) && e.code == MULIB_ASDL_NAME);
  m = mulib_asdl_parse("module Override {}", 18, &o, &e);
  assert(m && !strcmp(mulib_asdl_name(m), "Override")); mulib_asdl_destroy(m);
  o.module_name = NULL;
  assert(mulib_strbuf_append(&out, "unchanged"));
  assert(!mulib_asdl_compile("t =", 3, NULL, &out, &e) && !strcmp(out.data, "unchanged"));
  m = parse("t = A"); o.format = (mulib_asdl_format_t)42;
  assert(!mulib_asdl_generate(m, &o, &out, &e) && e.code == MULIB_ASDL_INVALID && out.length == 9);
  o.format = MULIB_ASDL_C99; o.c_prefix = "_bad";
  assert(!mulib_asdl_generate(m, &o, &out, &e) && e.code == MULIB_ASDL_INVALID && out.length == 9);
  assert(!mulib_asdl_generate(m, NULL, NULL, &e) && e.code == MULIB_ASDL_INVALID);
  mulib_asdl_destroy(m);
  memset(&o, 0, sizeof o); o.parser.max_steps = 2;
  assert(!mulib_asdl_parse("t=A", 3, &o, &e) && e.code == MULIB_ASDL_LIMIT);
  o.parser.max_steps = 0; o.parser.max_captures = 1;
  assert(!mulib_asdl_parse("t=A", 3, &o, &e) && e.code == MULIB_ASDL_LIMIT);
  o.parser.max_captures = 0; o.parser.max_depth = 1;
  assert(!mulib_asdl_parse("t=A", 3, &o, &e) && e.code == MULIB_ASDL_LIMIT);
  memset(longname, 'a', 255); strcpy(longname + 255, " = A");
  m = parse(longname); mulib_asdl_destroy(m);
  memset(longname, 'a', 256); strcpy(longname + 256, " = A"); malformed(longname, MULIB_ASDL_NAME);
  assert(!mulib_asdl_parse("\n t = A(unknown)", 16, NULL, &e));
  assert(e.code == MULIB_ASDL_NAME && e.line == 2 && e.column == 8);
  /* The semantic budget bounds quadratic uniqueness checks. */
  for (i = 0; i < 1500; ++i) {
    char line[64]; snprintf(line, sizeof line, "t%zu = C%zu\n", i, i);
    assert(mulib_strbuf_append(&many, line));
  }
  assert(!mulib_asdl_parse(many.data, many.length, NULL, &e) && e.code == MULIB_ASDL_LIMIT);
  mulib_strbuf_free(&many); mulib_strbuf_free(&out);
  assert(mulib_strbuf_append(&out, "t = A"));
  assert(mulib_asdl_compile(out.data, out.length, NULL, &out, &e));
  assert(!strncmp(out.data, "t = A(asdl", 10)); mulib_strbuf_free(&out);
}
static void test_mutations(void) {
  uint32_t seed = 17;
  size_t i, j, length;
  char bytes[sizeof fixture];
  for (i = 0; i < 1000; ++i) {
    mulib_asdl_error_t e;
    mulib_asdl_module_t *m;
    memcpy(bytes, fixture, sizeof fixture);
    for (j = 0; j < 3; ++j) {
      seed = seed * 1664525u + 1013904223u;
      bytes[seed % (sizeof fixture - 1)] = (char)(seed >> 24);
    }
    length = i % 2 ? sizeof fixture - 1 : seed % sizeof fixture;
    m = mulib_asdl_parse(bytes, length, NULL, &e);
    assert(m ? e.code == MULIB_ASDL_OK : e.code != MULIB_ASDL_OK);
    mulib_asdl_destroy(m);
  }
}
int main(int argc, char **argv) {
  if (argc == 2) {
    mulib_asdl_options_t o = {0};
    mulib_asdl_error_t e;
    mulib_strbuf_t b = {0};
    o.c_prefix = "test";
    if (!strcmp(argv[1], "--emit-c") || !strcmp(argv[1], "--emit-empty")) o.format = MULIB_ASDL_C99;
    else if (!strcmp(argv[1], "--emit-yantlr")) o.format = MULIB_ASDL_YANTLR;
    else assert(!strcmp(argv[1], "--emit-sexpr"));
    if (!strcmp(argv[1], "--emit-empty")) assert(mulib_asdl_compile(NULL, 0, &o, &b, &e));
    else assert(mulib_asdl_compile(fixture, sizeof fixture - 1, &o, &b, &e));
    assert(fwrite(b.data, 1, b.length, stdout) == b.length); mulib_strbuf_free(&b); return 0;
  }
  test_parser(); test_api(); test_formats(); test_errors_limits(); test_mutations();
  puts("ASDL: parser, API, validation and three backends passed"); return 0;
}
