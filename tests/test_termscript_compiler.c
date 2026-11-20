#ifndef MULIB_TERMSCRIPT_TEST_GENERATED
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMSCRIPT_AST
#define MULIB_IMPL_XMACIR
#define MULIB_IMPL_CAPASM
#define MULIB_IMPL_TERMSCRIPT_CAPASM
#include "mulib-termscript-capasm.h"
#endif
#define MULIB_IMPL_TERMSCRIPT_RUNTIME
#include "mulib-termscript-runtime.h"

#undef NDEBUG
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char output[1024];
  size_t length;
} trace_t;

static int
capture(mulib_termscript_vm_t *vm, void *userdata, const char *word, size_t n,
        mulib_termscript_error_t *error)
{
  trace_t *trace = (trace_t *)userdata;
  int escape = n == 4 && !memcmp(word, "emit", 4);
  if (!escape && (n != 7 || memcmp(word, "capture", 7))) return 0;
  mulib_termscript_value_t v;
  if (!mulib_termscript_pop(vm, &v)) {
    mulib_termscript_fail(vm, error, "capture needs a value");
    return -1;
  }
  if (escape) assert(v.kind == MULIB_TERMSCRIPT_VALUE_ESCAPE);
  const char *name = v.kind == MULIB_TERMSCRIPT_VALUE_INTEGER ? "int"
    : v.kind == MULIB_TERMSCRIPT_VALUE_STRING ? "str"
    : v.kind == MULIB_TERMSCRIPT_VALUE_COMMAND ? "cmd"
    : v.kind == MULIB_TERMSCRIPT_VALUE_CHARACTER ? "char"
    : v.kind == MULIB_TERMSCRIPT_VALUE_ESCAPE ? "esc" : "unexpected";
  int len = snprintf(trace->output + trace->length, sizeof(trace->output) - trace->length,
                     "%s:%" PRId64 ":", name, v.integer);
  assert(len >= 0 && (size_t)len < sizeof(trace->output) - trace->length);
  trace->length += (size_t)len;
  const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < v.length; ++i) {
    unsigned byte = (unsigned char)v.text[i];
    assert(trace->length + 3 < sizeof(trace->output));
    trace->output[trace->length++] = hex[byte >> 4];
    trace->output[trace->length++] = hex[byte & 15u];
  }
  trace->output[trace->length++] = '\n';
  trace->output[trace->length] = '\0';
  return 1;
}

static void
setup(mulib_termscript_vm_t *vm, trace_t *trace)
{
  mulib_termscript_vm_init(vm, NULL);
  mulib_termscript_vm_set_host(vm, capture, trace);
  assert(mulib_termscript_vm_set_term(vm, "xterm", 5));
}

static void
check_result(mulib_termscript_vm_t *vm, const trace_t *trace)
{
  const mulib_termscript_value_t *top = mulib_termscript_peek(vm, 0);
  assert(top && top->kind == MULIB_TERMSCRIPT_VALUE_INTEGER && top->integer == INT64_MIN);
  assert(mulib_termscript_stack_size(vm) == 1);
  assert(!strcmp(trace->output, "int:0:\n"
                               "str:0:610a00225c3f3f2ff09f9982\n"
                               "int:2:\n"
                               "cmd:0:7469636b60\n"
                                "int:11:\n"
                                "int:24:\n"
                                "int:9:\n"
                                "char:27:1b\n"
                                "esc:0:435550\n"));
}

static void
check_retained_key(mulib_termscript_vm_t *vm, const trace_t *trace)
{
  mulib_termscript_error_t e;
  assert(!mulib_termscript_key(vm, "alt+ctrl+F1", 11, &e));
  const size_t before = trace->length;
  assert(mulib_termscript_key(vm, "ctrl+alt+F1", 11, &e) == 1 && !e.message);
  assert(!strcmp(trace->output + before, "int:42:\n"));
  assert(mulib_termscript_stack_size(vm) == 1);
}

#ifdef MULIB_TERMSCRIPT_TEST_GENERATED

int compiler_fixture(mulib_termscript_vm_t *, mulib_termscript_error_t *);

int
main(void)
{
  mulib_termscript_vm_t vm;
  trace_t trace = { { 0 }, 0 };
  mulib_termscript_error_t e = { 0, NULL };
  setup(&vm, &trace);
  if (!compiler_fixture(&vm, &e)) {
    fprintf(stderr, "%zu: %s\n", e.offset, e.message ? e.message : "execution failed");
    mulib_termscript_vm_clear(&vm);
    return 1;
  }
  check_result(&vm, &trace);
  check_retained_key(&vm, &trace);
  fputs(trace.output, stdout);
  mulib_termscript_vm_clear(&vm);
  return 0;
}

#else

static const char fixture[] =
  "% proof that comments and original layout are not retained\n"
  "#i 0 set\n"
  "{ #i get 3 lt? } while {\n"
  "  #i get 1 eq? if { \"a\\n\\0\\\"\\\\?" "?/\xf0\x9f\x99\x82\" capture }\n"
  "  else { #i get capture }\n"
  "  #i 1 +\n"
  "} loop\n"
  "{ false } { \"unreachable\" capture } loop\n"
  "false { \"unreachable\" capture } { `tick\\`` capture } ifelse\n"
  "6 5 ($a $b) { $a + $b } lambda call capture\n"
  "%if-term: xterm\n"
  "  %if-term: \"an exact name\"\n"
  "    99 capture\n"
  "  %else:\n"
  "    7 5 + 2 mul capture\n"
  "  %end:\n"
  "%else:\n"
  "  100 capture\n"
  "%end:\n"
  "<a: [2, 9]> .a at 1 at capture\n"
  "#\\escape capture\n"
  "[[ CUP ]] emit\n"
  "[[ Alt, F1, !Ctrl ]] { 6 7 mul capture } bind\n"
  "-9223372036854775808\n";

static void
interpret(const mulib_termscript_program_t *p, trace_t *trace)
{
  mulib_termscript_vm_t vm;
  mulib_termscript_error_t e = { 0, NULL };
  setup(&vm, trace);
  if (!mulib_termscript_run(&vm, p, &e)) {
    fprintf(stderr, "%zu: %s\n", e.offset, e.message ? e.message : "execution failed");
    abort();
  }
  check_result(&vm, trace);
  check_retained_key(&vm, trace);
  mulib_termscript_vm_clear(&vm);
}

static void
canonical(void)
{
  mulib_termscript_program_t a = { 0 }, b = { 0 };
  mulib_termscript_error_t e = { 0, NULL };
  const char source[] = "% comment\n { +0007 5 + 2 mul } #\\escape 0 length;";
  assert(mulib_termscript_parse(source, sizeof source - 1, &a, &e));
  assert(mulib_termscript_canonicalize(&a, &b, &e));
  assert(b.root == 0 && b.count == 7 && b.length == 6 && !strcmp(b.source, "mullen"));
  assert(b.nodes[0].kind == MULIB_TERMSCRIPT_BLOCK && b.nodes[0].length == 0);
  assert(b.nodes[0].first == 1 && b.nodes[0].next == 4);
  assert(b.nodes[1].kind == MULIB_TERMSCRIPT_INTEGER && b.nodes[1].integer == 12);
  assert(b.nodes[2].kind == MULIB_TERMSCRIPT_INTEGER && b.nodes[2].integer == 2);
  assert(b.nodes[3].kind == MULIB_TERMSCRIPT_WORD && b.nodes[3].length == 3);
  assert(b.nodes[4].kind == MULIB_TERMSCRIPT_CHARACTER && b.nodes[4].integer == 27);
  assert(b.nodes[4].length == 0);
  /* Already canonical programs can be canonicalized in place. */
  assert(mulib_termscript_canonicalize(&b, &b, &e));
  assert(b.count == 7 && b.nodes[1].integer == 12 && !strcmp(b.source, "mullen"));

  mulib_termscript_node_t *saved = b.nodes;
  a.nodes[a.root].next = a.root;
  assert(!mulib_termscript_canonicalize(&a, &b, &e));
  assert(e.message && strstr(e.message, "AST link") && b.nodes == saved);
  a.nodes[a.root].next = SIZE_MAX;
  assert(!mulib_termscript_canonicalize(&a, &b, &e));
  assert(e.message && strstr(e.message, "unreachable") && b.nodes == saved);
  mulib_termscript_program_free(&a);
  mulib_termscript_program_free(&b);
}

static void
deep_ast(void)
{
  /* Above the text parser's depth limit: the AST emitter itself must not
   * recursively visit nodes or copy container text. */
  const size_t count = 4096;
  mulib_termscript_program_t ast = { NULL, 0, NULL, count, 0 };
  ast.nodes = (mulib_termscript_node_t *)malloc(count * sizeof(*ast.nodes));
  assert(ast.nodes);
  for (size_t i = 0; i < count; ++i) {
    mulib_termscript_node_t block = {
      MULIB_TERMSCRIPT_BLOCK, 0, 0, i + 1 == count ? SIZE_MAX : i + 1, SIZE_MAX, 0
    };
    ast.nodes[i] = block;
  }
  char *out = NULL;
  assert(mulib_termscript_emit_c11(&ast, "deep_fixture", &out, NULL));
  assert(strstr(out, "deep_fixture(") && strstr(out, "mulib_termscript_run_capsule"));
  free(out);
  mulib_termscript_program_free(&ast);
}

static void
same_failure(const char *text, const char *expected)
{
  mulib_termscript_program_t src = { 0 }, optimized = { 0 };
  mulib_termscript_error_t e = { 0, NULL };
  mulib_termscript_vm_t a, b;
  assert(mulib_termscript_parse(text, strlen(text), &src, &e));
  assert(mulib_termscript_canonicalize(&src, &optimized, &e));
  mulib_termscript_vm_init(&a, NULL);
  mulib_termscript_vm_init(&b, NULL);
  assert(!mulib_termscript_run(&a, &src, &e));
  assert(e.message && !strcmp(e.message, expected));
  assert(!mulib_termscript_run(&b, &optimized, &e));
  assert(e.message && !strcmp(e.message, expected));
  size_t n = mulib_termscript_stack_size(&a);
  assert(mulib_termscript_stack_size(&b) == n);
  for (size_t i = 0; i < n; ++i) {
    const mulib_termscript_value_t *original = mulib_termscript_peek(&a, i);
    const mulib_termscript_value_t *replay = mulib_termscript_peek(&b, i);
    assert(original->kind == replay->kind && original->integer == replay->integer);
  }
  mulib_termscript_vm_clear(&a);
  mulib_termscript_vm_clear(&b);
  mulib_termscript_program_free(&src);
  mulib_termscript_program_free(&optimized);
}

static void
negative(void)
{
  static const char *const names[] = {
    NULL, "", "0fixture", "_fixture", "int", "main", "foo/**/", "foo\n", "mulib_termscript_run",
    "MULIB_TERMSCRIPT_WORD", "INT64_C", "size_t"
  };
  static const struct { const char *script, *message; size_t offset; } bad[] = {
    { "{", "unclosed", 1 },
    { "%else:\n", "%else:", 0 },
    { "%if-term: x\n", "unterminated", 0 },
    { "%if-term: \"x\n%end:\n", "operand", 0 },
    { "%if-term: x\n%else:\n%else:\n%end:\n", "%else:", 19 },
    { "%if-term: x\n{ %end:\n}\n", "%end:", 14 },
    { "true if {} else 1", "else requires a block", 11 }
  };
  char sentinel[] = "unchanged", *out = sentinel;
  mulib_termscript_error_t e = { 0, NULL };
  for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
    assert(!mulib_termscript_compile_c11("", 0, names[i], &out, &e));
    assert(out == sentinel && e.offset == 0 && e.message && strstr(e.message, "identifier"));
  }
  for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
    const char *text = bad[i].script;
    assert(!mulib_termscript_compile_c11(text, strlen(text), "fixture", &out, &e));
    assert(out == sentinel && e.offset == bad[i].offset && e.message && strstr(e.message, bad[i].message));
  }
  assert(mulib_termscript_compile_c11(NULL, 0, "empty_fixture", &out, &e));
  assert(!e.message && strstr(out, "#include <stddef.h>") && strstr(out, "empty_fixture("));
  free(out);
  char longest[65];
  memset(longest, 't', sizeof longest);
  longest[64] = '\0';
  assert(!mulib_termscript_compile_c11(NULL, 0, longest, &out, &e));
  longest[63] = '\0';
  assert(mulib_termscript_compile_c11(NULL, 0, longest, &out, &e));
  assert(strstr(out, longest));
  free(out);

  same_failure("9223372036854775807 1 +", "integer overflow");
  same_failure("0 -9223372036854775808 -", "integer overflow");
  same_failure("-9223372036854775808 -1 *", "integer overflow");
  same_failure("-9223372036854775808 -1 /", "integer overflow");
  same_failure("-9223372036854775808 -1 mod", "integer overflow");
  same_failure("2 3 + 4 * 0 /", "division by zero");
  same_failure("2 3 + 4 * node", "value has no source node");
}

static unsigned char *
wire(const char *text, size_t *length)
{
  mulib_termscript_program_t src = { 0 }, canonical = { 0 };
  assert(mulib_termscript_parse(text, strlen(text), &src, NULL));
  assert(mulib_termscript_canonicalize(&src, &canonical, NULL));
  unsigned char *bytes = mulib_termscript_compiler_encode_(&canonical, length, NULL);
  assert(bytes);
  mulib_termscript_program_free(&src);
  mulib_termscript_program_free(&canonical);
  return bytes;
}

static void
capsule(void)
{
  size_t n;
  unsigned char *bytes = wire("99 capture { 8 } \"x\" %if-term: xterm\n%end:\n", &n);
  mulib_termscript_vm_t vm;
  mulib_termscript_error_t e;
  trace_t trace = { { 0 }, 0 };
  setup(&vm, &trace);
  for (size_t i = 0; i < n; ++i)
    assert(!mulib_termscript_run_capsule(&vm, &e, bytes, i) && e.message);
  assert(!mulib_termscript_run_capsule(&vm, &e, NULL, n) && e.message);

  static const size_t corrupt[] = {
    5, 15, 23, 24, /* version, count, pool, root */
    MULIB_TSCAP_HEADER_ + 2 * MULIB_TSCAP_NODE_ + MULIB_TSCAP_FIRST_,
    MULIB_TSCAP_HEADER_ + 3 * MULIB_TSCAP_NODE_ + MULIB_TSCAP_PARENT_,
    MULIB_TSCAP_HEADER_ + 4 * MULIB_TSCAP_NODE_ + MULIB_TSCAP_KIND_,
    MULIB_TSCAP_HEADER_ + 4 * MULIB_TSCAP_NODE_ + MULIB_TSCAP_LENGTH_,
    MULIB_TSCAP_HEADER_ + 4 * MULIB_TSCAP_NODE_ + MULIB_TSCAP_OFFSET_,
    MULIB_TSCAP_HEADER_ + 4 * MULIB_TSCAP_NODE_ + MULIB_TSCAP_NEXT_ + 7,
    MULIB_TSCAP_HEADER_ + 4 * MULIB_TSCAP_NODE_ + MULIB_TSCAP_INTEGER_
  };
  for (size_t i = 0; i < sizeof corrupt / sizeof *corrupt; ++i) {
    bytes[corrupt[i]] ^= 0x80;
    assert(!mulib_termscript_run_capsule(&vm, &e, bytes, n));
    assert(e.message && strstr(e.message, "invalid TermScript capsule"));
    bytes[corrupt[i]] ^= 0x80;
  }
  bytes[n - 5] = '!';
  assert(!mulib_termscript_run_capsule(&vm, &e, bytes, n) && e.message);
  bytes[n - 5] = '%';
  const size_t string = MULIB_TSCAP_HEADER_ + 4 * MULIB_TSCAP_NODE_;
  const size_t x = n - (size_t)mulib_tscap_read_(bytes + 16)
    + (size_t)mulib_tscap_read_(bytes + string + MULIB_TSCAP_OFFSET_);
  assert(bytes[x] == 'x');
  bytes[x] = 0xf4;
  assert(!mulib_termscript_run_capsule(&vm, &e, bytes, n) && e.offset == x);
  bytes[x] = '\\';
  assert(!mulib_termscript_run_capsule(&vm, &e, bytes, n) && e.offset == x);
  bytes[x] = 'x';
  assert(!trace.length && !mulib_termscript_stack_size(&vm));
  assert(mulib_termscript_run_capsule(&vm, &e, bytes, n) && !e.message);
  assert(!strcmp(trace.output, "int:99:\n"));
  free(bytes);
  mulib_termscript_vm_clear(&vm);

  /* A failed run can still leave a quotation bound. Its program must
   * survive the capsule buffer and remain valid until VM clear. */
  bytes = wire("[[Shift, F2]] { \"later\" } bind no-such-word", &n);
  mulib_termscript_vm_init(&vm, NULL);
  assert(!mulib_termscript_run_capsule(&vm, &e, bytes, n) && e.message);
  memset(bytes, 0, n);
  free(bytes);
  assert(mulib_termscript_key(&vm, "shift+F2", 8, &e) == 1 && !e.message);
  const mulib_termscript_value_t *value = mulib_termscript_peek(&vm, 0);
  assert(value && value->program && value->kind == MULIB_TERMSCRIPT_VALUE_STRING);
  assert(value->length == 5 && !memcmp(value->text, "later", 5));
  mulib_termscript_vm_clear(&vm);

  bytes = wire("-9223372036854775808 -1 1 9223372036854775807", &n);
  mulib_termscript_vm_init(&vm, NULL);
  assert(mulib_termscript_run_capsule(&vm, &e, bytes, n) && !e.message);
  const int64_t expected[] = { INT64_MAX, 1, -1, INT64_MIN };
  for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i) {
    const mulib_termscript_value_t *integer = mulib_termscript_peek(&vm, i);
    assert(integer && integer->kind == MULIB_TERMSCRIPT_VALUE_INTEGER
           && integer->integer == expected[i]);
  }
  mulib_termscript_vm_clear(&vm);
  free(bytes);

  bytes = wire("", &n);
  const mulib_termscript_limits_t limits = { 0, 0, 512, 0 };
  mulib_termscript_vm_init(&vm, &limits);
  size_t runs = 0;
  while (runs < 64 && mulib_termscript_run_capsule(&vm, &e, bytes, n)) ++runs;
  assert(runs > 0 && runs < 64 && e.message && strstr(e.message, "allocation limit"));
  mulib_termscript_vm_clear(&vm);
  assert(mulib_termscript_run_capsule(&vm, NULL, bytes, n));
  mulib_termscript_vm_clear(&vm);
  free(bytes);
}

static int
record_build(void *arg, mulib_capasm_stage_t stage, const mulib_capasm_tool_t *tool,
             const char *input, size_t length, char **out, size_t *n, mulib_xmacir_error_t *error)
{
  static const char artifact[] = { 'A', '\0', 'S', 'M' };
  size_t *calls = (size_t *)arg;
  assert(stage == MULIB_CAPASM_COMPILE && !strcmp(tool->executable, "mock-cc"));
  assert(length && strstr(input, "#include <stddef.h>") && strstr(input, "options_fixture("));
  ++*calls;
  if (*calls == 2) {
    error->offset = 71;
    error->message = "mock compiler failure";
    return 0;
  }
  *out = (char *)malloc(sizeof artifact);
  assert(*out);
  memcpy(*out, artifact, sizeof artifact);
  *n = sizeof artifact;
  return 1;
}

static void
options(void)
{
  const mulib_capasm_tool_t tool = { "compiler", "mock-cc", NULL, 0 };
  size_t calls = 0, n = 75;
  const mulib_capasm_options_t opts = { .compiler = &tool, .run = record_build, .user = &calls };
  char sentinel[] = "unchanged", *out = sentinel;
  mulib_termscript_error_t e;
  assert(mulib_termscript_compile("1", 1, "options_fixture", &opts, &out, &n, &e));
  assert(calls == 1 && !e.message && n == 4 && !memcmp(out, "A\0SM\0", 5));
  free(out);
  out = sentinel;
  assert(!mulib_termscript_compile("1", 1, "options_fixture", &opts, &out, &n, &e));
  assert(calls == 2 && out == sentinel && n == 4 && e.offset == 71 && e.message);
}

int
main(int argc, char **argv)
{
  mulib_termscript_program_t src = { 0 }, opt = { 0 };
  mulib_termscript_error_t e = { 0, NULL };
  assert(argc <= 2);
  if (argc == 2 && !strcmp(argv[1], "--emit")) {
    char *output = NULL;
    size_t length = 0;
    assert(mulib_termscript_compile(fixture, sizeof fixture - 1, "compiler_fixture",
                                     NULL, &output, &length, &e));
    assert(length == strlen(output));
    assert(!strstr(output, "mulib-termscript-runtime.h") && !strstr(output, "INT64_"));
    fputs(output, stdout);
    free(output);
    return 0;
  }
  trace_t baseline = { { 0 }, 0 };
  assert(mulib_termscript_parse(fixture, sizeof fixture - 1, &src, &e));
  interpret(&src, &baseline);
  if (argc == 2) {
    assert(!strcmp(argv[1], "--interpret"));
    fputs(baseline.output, stdout);
  } else {
    trace_t compiled = { { 0 }, 0 };
    assert(mulib_termscript_canonicalize(&src, &opt, &e));
    assert(opt.count < src.count && opt.length < src.length);
    interpret(&opt, &compiled);
    assert(!strcmp(baseline.output, compiled.output));
    canonical();
    deep_ast();
    negative();
    capsule();
    options();
  }
  mulib_termscript_program_free(&opt);
  mulib_termscript_program_free(&src);
  return 0;
}

#endif
