#define MULIB_IMPL_VIZIGEN
#define MULIB_IMPL_ASDL
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_BINFILEREADER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_WALKER
#define MULIB_IMPL_EVTLISTENER
#define MULIB_IMPL_EVTSTREAM
#define MULIB_IMPL_AST
#define MULIB_IMPL_MMFILE
#include "mulib-vizigen.h"
#include <assert.h>

static const char schema[] =
  "module Trees {\n"
  " -- @enter(ignored)\n"
  " expr = Num(int value) @enter(number) @leave(done)\n"
  "      | Add(expr left, expr right) @enter(add) @leave(done)\n"
  "      | Empty attributes (int? line)\n"
  " pair = (string? name, expr* values) @enter(pair)\n"
  "}\n";
typedef struct log_data { char text[256]; size_t used; int result; } log_data;
static int record(const mulib_event_t *e, void *user) {
  log_data *log = (log_data *)user;
  int n = snprintf(log->text + log->used, sizeof log->text - log->used,
      "%c%s:%zu;", e->kind == MULIB_EVENT_ENTER ? '+' : '-', e->node->production, e->depth);
  assert(n > 0 && (size_t)n < sizeof log->text - log->used);
  log->used += (size_t)n; return log->result;
}
static void compile(const char *s, mulib_buffer_t *b) {
  mulib_asdl_error_t error;
  int ok = mulib_vizigen_compile(s, strlen(s), NULL, b, &error);
  if (!ok) fprintf(stderr, "%zu:%zu %s\n", error.line, error.column, error.message);
  assert(ok && !error.code);
}
static void bad(const char *s, mulib_asdl_error_code_t code) {
  mulib_buffer_t b = {NULL, 0, 0};
  mulib_asdl_error_t error;
  assert(mulib_buffer_append(&b, "sentinel", 8));
  assert(!mulib_vizigen_compile(s, strlen(s), NULL, &b, &error));
  if (error.code != code) fprintf(stderr, "%s: %d %s\n", s, error.code, error.message);
  assert(error.code == code && error.message[0] && error.line && error.column);
  assert(error.span.offset <= strlen(s));
  assert(b.length == 8 && !memcmp(b.data, "sentinel", 8));
  mulib_buffer_free(&b);
}
static void test_compiler(void) {
  mulib_buffer_t b = {NULL, 0, 0}, other = {NULL, 0, 0};
  mulib_vizigen_program_t p;
  mulib_asdl_error_t error;
  mulib_asdl_module_t *m;
  mulib_vizigen_rule_t rules[] = {{"A", MULIB_EVENT_ENTER, "foo"}};
  compile(schema, &b); assert(mulib_vizigen_open(&p, b.data, b.length));
  compile(schema, &other); assert(b.length == other.length && !memcmp(b.data, other.data, b.length));
  mulib_buffer_free(&b); mulib_buffer_free(&other);
  assert(mulib_vizigen_compile(NULL, 0, NULL, &b, NULL));
  assert(b.length == 16 && mulib_vizigen_open(&p, b.data, b.length));
  mulib_buffer_free(&b);
  compile("module Plain { t = A(t? child, t* children) | B attributes (int line) p = (identifier) }", &b);
  assert(b.length == 16); mulib_buffer_free(&b);
  compile("t = A /* @nonsense */ @enter(foo) // @leave(ignored)\n | B", &b);
  assert(b.length > 16); mulib_buffer_free(&b);
  compile("t = A @enter(foo) attributes (int x) p = (int) @leave(bar)", &b);
  assert(b.length > 16); mulib_buffer_free(&b);
  { char boundary[300];
    memcpy(boundary, "t=A @enter(", 11); memset(boundary + 11, 'a', 255);
    boundary[266] = ')'; boundary[267] = 0;
    compile(boundary, &b); mulib_buffer_free(&b);
    boundary[266] = 'a'; boundary[267] = ')'; boundary[268] = 0;
    bad(boundary, MULIB_ASDL_SYNTAX);
  }
  /* Source may alias the binary destination, including across reallocation. */
  assert(mulib_buffer_append(&b, schema, sizeof schema - 1));
  assert(mulib_vizigen_compile((const char *)b.data, b.length, NULL, &b, &error));
  assert(mulib_vizigen_open(&p, b.data + sizeof schema - 1, b.length - sizeof schema + 1));
  mulib_buffer_free(&b);
  bad("t = A @enter(foo) @enter(bar)", MULIB_ASDL_DUPLICATE);
  bad("t = A @exit(foo)", MULIB_ASDL_SYNTAX);
  bad("t = A @enter()", MULIB_ASDL_SYNTAX);
  bad("t = A @enter(_foo)", MULIB_ASDL_SYNTAX);
  bad("t = A @enter(foo", MULIB_ASDL_SYNTAX);
  bad("t = A { arbitrary C; }", MULIB_ASDL_SYNTAX);
  bad("@enter(foo) t = A", MULIB_ASDL_SYNTAX);
  bad("t = A(int @enter(foo) value)", MULIB_ASDL_SYNTAX);
  bad("p = (int @enter(foo) value)", MULIB_ASDL_SYNTAX);
  bad("t = A attributes (int line) @leave(foo)", MULIB_ASDL_SYNTAX);
  bad("module T { t = A } @enter(foo)", MULIB_ASDL_SYNTAX);
  bad("t = A /* unfinished", MULIB_ASDL_SYNTAX);
  bad("t = A(unknown x) @leave(foo)", MULIB_ASDL_NAME);
  bad("p = (p next) @leave(foo)", MULIB_ASDL_CYCLE);
  bad("t = A | A", MULIB_ASDL_DUPLICATE);
  assert(!mulib_vizigen_compile(NULL, 1, NULL, &b, &error) && error.code == MULIB_ASDL_INVALID);
  assert(!mulib_vizigen_compile("", 16777217u, NULL, &b, &error) && error.code == MULIB_ASDL_LIMIT);
  assert(!mulib_vizigen_compile("", 0, NULL, NULL, NULL));
  assert(!mulib_vizigen_compile("t=A\0", 4, NULL, &b, &error));
  assert(!mulib_vizigen_compile("t=A/*\0*/", 8, NULL, &b, &error));
  m = mulib_asdl_parse("t = A", 5, NULL, &error); assert(m);
  assert(mulib_vizigen_generate(m, rules, 1, &b, &error));
  rules[0].production = "t";
  assert(!mulib_vizigen_generate(m, rules, 1, &b, &error) && error.code == MULIB_ASDL_NAME);
  rules[0].production = "A"; rules[0].kind = (mulib_event_kind_t)2;
  assert(!mulib_vizigen_generate(m, rules, 1, &b, &error) && error.code == MULIB_ASDL_INVALID);
  assert(!mulib_vizigen_generate(m, rules, 65537u, &b, &error) && error.code == MULIB_ASDL_LIMIT);
  assert(!mulib_vizigen_generate(NULL, NULL, 0, &b, NULL));
  mulib_asdl_destroy(m); mulib_buffer_free(&b);
  /* Error coordinates refer to the unmodified annotated source. */
  assert(!mulib_vizigen_compile("t = A\n @bogus(x)", 16, NULL, &b, &error));
  assert(error.line == 2 && error.column == 2 && error.span.offset == 7);
}
static void test_execution(void) {
  mulib_buffer_t b = {NULL, 0, 0};
  mulib_vizigen_program_t p;
  mulib_vizigen_runtime_t rt;
  log_data log = {{0}, 0, 1};
  mulib_vizigen_action_t actions[] = {
    {"number", record, &log}, {"add", record, &log},
    {"done", record, &log}, {"pair", record, &log}
  };
  mulib_ast_t *root = mulib_ast_create("Add", NULL, 0);
  mulib_ast_t *left = mulib_ast_create("Num", "1", 1), *right = mulib_ast_create("Num", "2", 1);
  mulib_evtlistener_t listener;
  mulib_event_t e;
  /* A file parser can use a stack-local node with no AST links/allocations. */
  mulib_ast_t temporary = {0};
  const char *names[] = {"Add", "Num", "Num", "Num", "Num", "Add"};
  const mulib_event_kind_t kinds[] = {MULIB_EVENT_ENTER, MULIB_EVENT_ENTER, MULIB_EVENT_LEAVE,
      MULIB_EVENT_ENTER, MULIB_EVENT_LEAVE, MULIB_EVENT_LEAVE};
  size_t depths[] = {0, 1, 1, 1, 1, 0}, i;
  char expected[256];
  assert(root && left && right && mulib_ast_append(root, left) && mulib_ast_append(root, right));
  compile(schema, &b); assert(mulib_vizigen_open(&p, b.data, b.length));
  assert(!mulib_vizigen_runtime_init(&rt, &p, actions, 2));
  assert(!mulib_vizigen_runtime_init(&rt, &p, NULL, 1));
  assert(mulib_vizigen_runtime_init(&rt, &p, actions, 4));
  assert(mulib_vizigen_walk(&rt, root) == 1);
  assert(!strcmp(log.text, "+Add:0;+Num:1;-Num:1;+Num:1;-Num:1;-Add:0;"));
  strcpy(expected, log.text); log.used = 0; log.text[0] = 0;
  listener = mulib_vizigen_listener(&rt);
  for (i = 0; i < 6; ++i) {
    temporary.production = (char *)names[i]; temporary.text = (char *)"value"; temporary.length = 5;
    e.node = &temporary; e.kind = kinds[i]; e.depth = depths[i];
    assert(mulib_evtlistener_push(&listener, &e) == 1);
  }
  assert(!strcmp(log.text, expected));
  log.used = 0; log.text[0] = 0;
  /* Attached subtrees get their own zero-based traversal depths. */
  assert(mulib_vizigen_walk(&rt, left) == 1 && !strcmp(log.text, "+Num:0;-Num:0;"));
  { mulib_evtstream_t stream = mulib_evtstream_from_ast(root);
    log.used = 0; log.text[0] = 0; log.result = 0;
    assert(mulib_evtlistener_listen(&stream, &listener) == 0);
    log.result = 1;
    assert(mulib_evtlistener_listen(&stream, &listener) == 1 && !strcmp(log.text, expected));
    assert(mulib_evtlistener_listen(&stream, &listener) == 1); /* Sticky exhaustion. */
    assert(mulib_evtlistener_listen(NULL, &listener) == -1);
    assert(mulib_evtlistener_listen(&stream, NULL) == -1);
    assert(mulib_walker_walk(NULL, NULL) == -1);
  }
  log.used = 0; log.text[0] = 0; log.result = 0;
  assert(mulib_vizigen_walk(&rt, root) == 0 && !strcmp(log.text, "+Add:0;"));
  log.used = 0; log.text[0] = 0; log.result = -1;
  assert(mulib_vizigen_walk(&rt, root) == -1);
  log.result = 1;
  assert(mulib_vizigen_walk(&rt, NULL) == 1);
  assert(mulib_vizigen_walk(NULL, root) == -1);
  assert(mulib_vizigen_dispatch(NULL, &e) == -1);
  e.kind = (mulib_event_kind_t)2; assert(mulib_vizigen_dispatch(&rt, &e) == -1);
  assert(mulib_evtlistener_push(&listener, &e) == -1);
  e.kind = MULIB_EVENT_ENTER; temporary.production = (char *)"unknown";
  assert(mulib_evtlistener_push(&listener, &e) == 1);
  assert(mulib_evtlistener_push(NULL, &e) == -1);
  actions[3].name = "done"; /* Duplicate names rejected without executing. */
  assert(!mulib_vizigen_runtime_init(&rt, &p, actions, 4));
  actions[3].name = "pair"; actions[3].call = NULL;
  assert(!mulib_vizigen_runtime_init(&rt, &p, actions, 4));
  mulib_ast_destroy(root); mulib_buffer_free(&b);
}
static void test_binary(void) {
  mulib_buffer_t b = {NULL, 0, 0}, saved = {NULL, 0, 0};
  mulib_vizigen_program_t p, q, before;
  mulib_binfilereader_t r;
  mulib_mmfile_t map = MULIB_MMFILE_INIT;
  FILE *file;
  unsigned char *unaligned, old;
  size_t i, position;
  compile("t = A @enter(foo)", &b);
  assert(b.length == 34 && !memcmp(b.data, "VIZI\1\0\0\0\42\0\0\0\1\0\0\0", 16));
  assert(mulib_vizigen_open(&p, b.data, b.length));
  assert(mulib_vizigen_serialize(&p, &saved));
  assert(mulib_vizigen_serialize(&p, &saved));
  assert(mulib_binfilereader_init(&r, saved.data, saved.length));
  assert(mulib_vizigen_deserialize(&r, &q) && r.position == b.length);
  assert(q.bytes.data == saved.data && !memcmp(q.bytes.data, b.data, b.length));
  assert(mulib_vizigen_deserialize(&r, &q) && r.position == saved.length);
  before = q; position = r.position;
  assert(!mulib_vizigen_deserialize(&r, &q) && r.position == position && q.bytes.data == before.bytes.data);
  for (i = 0; i < b.length; ++i) {
    q = p;
    assert(!mulib_vizigen_open(&q, b.data, i) && q.bytes.data == p.bytes.data && q.bytes.size == p.bytes.size);
    assert(mulib_binfilereader_init(&r, b.data, i));
    assert(!mulib_vizigen_deserialize(&r, &q) && !r.position);
  }
  unaligned = (unsigned char *)malloc(b.length + 1); assert(unaligned);
  memcpy(unaligned + 1, b.data, b.length);
  assert(mulib_vizigen_open(&q, unaligned + 1, b.length)); free(unaligned);
  /* Bad magic, version, length, count, opcode, string lengths and NULs. */
  { const size_t offsets[] = {0, 4, 8, 12, 16, 20, 24, 28, 29, 30, 33};
    for (i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
      old = b.data[offsets[i]]; b.data[offsets[i]] = 255;
      assert(!mulib_vizigen_open(&q, b.data, b.length)); b.data[offsets[i]] = old;
    }
  }
  assert(!mulib_vizigen_open(NULL, b.data, b.length));
  assert(!mulib_vizigen_open(&q, NULL, 16));
  assert(!mulib_vizigen_open(&q, b.data, 67108865u));
  { uint32_t state = 17;
    for (i = 0; i < 1024; ++i) {
      size_t at;
      state = state * 1664525u + 1013904223u; at = state % b.length;
      old = b.data[at]; b.data[at] ^= (unsigned char)(state >> 24);
      (void)mulib_vizigen_open(&q, b.data, b.length); b.data[at] = old;
    }
  }
  assert(!mulib_vizigen_from_mapping(&q, &map));
  /* A portable externally assembled program can repeat a guarded CALL. */
  { mulib_buffer_t repeated = {NULL, 0, 0};
    log_data log = {{0}, 0, 2};
    mulib_vizigen_action_t action = {"foo", record, &log};
    mulib_vizigen_runtime_t rt;
    mulib_ast_t node = {0};
    mulib_event_t event = {MULIB_EVENT_ENTER, &node, 0};
    node.production = (char *)"A";
    assert(mulib_buffer_append(&repeated, b.data, b.length));
    assert(mulib_buffer_append(&repeated, b.data + 16, b.length - 16));
    assert(mulib_binfilewriter_patch(&repeated, 8, 4, repeated.length));
    assert(mulib_binfilewriter_patch(&repeated, 12, 4, 2));
    assert(mulib_vizigen_open(&q, repeated.data, repeated.length));
    assert(mulib_vizigen_runtime_init(&rt, &q, &action, 1));
    assert(mulib_vizigen_dispatch(&rt, &event) == 1 && !strcmp(log.text, "+A:0;+A:0;"));
    mulib_buffer_free(&repeated);
  }
  file = fopen("vizigen-test.bin", "wb"); assert(file);
  assert(fwrite(b.data, 1, b.length, file) == b.length); assert(!fclose(file));
#if defined(__unix__) || defined(__APPLE__)
  assert(mulib_mmfile_open(&map, "vizigen-test.bin", 0) == MULIB_MMFILE_OK);
  assert(mulib_vizigen_from_mapping(&q, &map));
  assert(q.bytes.data == map.data && q.bytes.size == b.length);
  { log_data log = {{0}, 0, 1};
    mulib_vizigen_action_t action = {"foo", record, &log};
    mulib_vizigen_runtime_t rt;
    mulib_ast_t node = {0};
    mulib_event_t event = {MULIB_EVENT_ENTER, &node, 0};
    node.production = (char *)"A";
    assert(mulib_vizigen_runtime_init(&rt, &q, &action, 1));
    assert(mulib_vizigen_dispatch(&rt, &event) == 1 && !strcmp(log.text, "+A:0;"));
  }
  assert(mulib_mmfile_close(&map) == MULIB_MMFILE_OK);
#endif
  assert(!remove("vizigen-test.bin"));
  mulib_buffer_free(&b); mulib_buffer_free(&saved);
}
static void test_work_limits(void) {
  char names[1500][16];
  mulib_asdl_constructor_t constructors[1024];
  mulib_vizigen_rule_t rules[1024];
  mulib_vizigen_action_t actions[1500];
  mulib_asdl_type_t type;
  mulib_asdl_module_t *m;
  mulib_asdl_error_t error;
  mulib_buffer_t b = {NULL, 0, 0};
  mulib_vizigen_program_t p;
  mulib_vizigen_runtime_t rt;
  size_t i;
  for (i = 0; i < 1500; ++i) {
    snprintf(names[i], sizeof names[i], "C%zu", i);
    actions[i].name = names[i]; actions[i].call = record; actions[i].user = NULL;
    if (i < 1024) {
      constructors[i] = mulib_asdl_constructor(names[i], NULL, 0);
      rules[i].production = names[i]; rules[i].kind = MULIB_EVENT_ENTER; rules[i].action = "foo";
    }
  }
  type = mulib_asdl_sum("t", constructors, 1024, NULL, 0);
  m = mulib_asdl_from_types("Limits", &type, 1, &error); assert(m);
  assert(!mulib_vizigen_generate(m, rules, 1024, &b, &error));
  assert(error.code == MULIB_ASDL_LIMIT && !b.length && !b.data);
  mulib_asdl_destroy(m);
  compile("", &b); assert(mulib_vizigen_open(&p, b.data, b.length));
  assert(mulib_vizigen_runtime_init(&rt, &p, NULL, 0));
  assert(!mulib_vizigen_runtime_init(&rt, &p, actions, 1500));
  assert(!rt.count && !rt.actions && rt.program.bytes.data == p.bytes.data);
  mulib_buffer_free(&b);
}
static void test_plain_asdl_compatibility(void) {
  static const char plain[] =
    "module T { t = A(int value) | B(t? child) attributes (string file) p = (t* items) }";
  char mutated[sizeof plain];
  uint32_t state = 31;
  size_t i;
  for (i = 0; i < 128; ++i) {
    mulib_buffer_t bytes = {NULL, 0, 0};
    mulib_asdl_module_t *module;
    mulib_vizigen_program_t program;
    int ok;
    memcpy(mutated, plain, sizeof plain);
    state = state * 1664525u + 1013904223u;
    mutated[state % (sizeof plain - 1)] = (char)(state >> 24);
    module = mulib_asdl_parse(mutated, sizeof plain - 1, NULL, NULL);
    ok = mulib_vizigen_compile(mutated, sizeof plain - 1, NULL, &bytes, NULL);
    assert(ok == (module != NULL));
    if (ok) assert(mulib_vizigen_open(&program, bytes.data, bytes.length) && bytes.length == 16);
    else assert(!bytes.length && !bytes.data);
    mulib_asdl_destroy(module); mulib_buffer_free(&bytes);
  }
}
int main(void) {
  test_compiler(); test_execution(); test_binary(); test_work_limits();
  test_plain_asdl_compatibility(); return 0;
}
