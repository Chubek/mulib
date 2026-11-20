#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_TERMSCRIPT_AST
#define MULIB_IMPL_TERMSCRIPT_RUNTIME
#include "mulib-termscript-runtime.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  mulib_termscript_vm_t vm;
  mulib_termscript_program_t programs[32];
  size_t count;
} fixture_t;

static const mulib_termscript_program_t *
parse(fixture_t *f, const char *source)
{
  assert(f->count < sizeof f->programs / sizeof *f->programs);
  mulib_termscript_program_t *p = &f->programs[f->count++];
  mulib_termscript_error_t err;
  if (!mulib_termscript_parse(source, strlen(source), p, &err)) {
    fprintf(stderr, "parse: %zu: %s\n", err.offset, err.message);
    assert(0);
  }
  return p;
}

static void
run(fixture_t *f, const char *source)
{
  mulib_termscript_error_t err;
  if (!mulib_termscript_run(&f->vm, parse(f, source), &err)) {
    fprintf(stderr, "run: %zu: %s: %s\n", err.offset, err.message, source);
    assert(0);
  }
  assert(!err.message);
}

static void
reject(fixture_t *f, const char *source, const char *message, size_t offset)
{
  mulib_termscript_error_t err;
  assert(!mulib_termscript_run(&f->vm, parse(f, source), &err));
  assert(err.message && strstr(err.message, message));
  if (offset != SIZE_MAX) assert(err.offset == offset);
}

static void
clear(fixture_t *f)
{
  mulib_termscript_vm_clear(&f->vm);
  mulib_termscript_vm_clear(&f->vm);
  for (size_t i = 0; i < f->count; ++i) mulib_termscript_program_free(&f->programs[i]);
  f->count = 0;
}

static mulib_termscript_value_t
pop(fixture_t *f)
{
  mulib_termscript_value_t v;
  assert(mulib_termscript_pop(&f->vm, &v));
  return v;
}

static void
integer(fixture_t *f, int64_t expected)
{
  mulib_termscript_value_t v = pop(f);
  assert(v.kind == MULIB_TERMSCRIPT_VALUE_INTEGER && v.integer == expected);
}

static void
boolean(fixture_t *f, int expected)
{
  mulib_termscript_value_t v = pop(f);
  assert(v.kind == MULIB_TERMSCRIPT_VALUE_BOOL && v.integer == expected);
}

static void
text(fixture_t *f, mulib_termscript_value_kind_t kind, const char *expected, size_t n)
{
  mulib_termscript_value_t v = pop(f);
  assert(v.kind == kind && v.length == n && !memcmp(v.text, expected, n));
}

static void
test_stack_and_arithmetic(void)
{
  fixture_t f = {0};
  mulib_termscript_vm_init(&f.vm, NULL);
  run(&f, "1 2 3 rot swap over add nip");
  integer(&f, 4);
  integer(&f, 2);
  run(&f, "2 dup mul 3 sub 2 add 2 div 2 mod neg");
  integer(&f, -1);
  run(&f, "8 5 gt? 0 -1 ge? and not 4 4 ne? or");
  boolean(&f, 0);
  run(&f, "\"xy\" \"xyz\" lt? \"hi\" \"hi\" eq? and #\\A #\\B ne? and");
  boolean(&f, 1);
  reject(&f, "9223372036854775807 1 add", "overflow", 22);
  integer(&f, 1);
  integer(&f, INT64_MAX);
  reject(&f, "1 0 div", "division by zero", 4);
  integer(&f, 0);
  integer(&f, 1);
  reject(&f, "-9223372036854775808 -1 div", "overflow", 24);
  reject(&f, "clear -9223372036854775808 -1 mul", "overflow", SIZE_MAX);
  reject(&f, "clear -9223372036854775808 1 sub", "overflow", SIZE_MAX);
  reject(&f, "clear -9223372036854775808 neg", "overflow", SIZE_MAX);
  reject(&f, "clear drop", "underflow", 6);
  reject(&f, "\"text\" 2 add", "expected integers", 9);
  clear(&f);
}

static void
test_language(void)
{
  fixture_t f = {0};
  mulib_termscript_vm_init(&f.vm, NULL);
  run(&f, "#i 0 set; { #i 10 lt? } while { #i 1 add; } loop; #i get");
  integer(&f, 10);
  run(&f, "#i 0 set 3 { #i 2 add } repeat i "
      "false if { 1 unknown } else { true { 42 } if } "
      "{ 1 } { 2 add } compose invoke");
  integer(&f, 3);
  integer(&f, 42);
  integer(&f, 6);
  run(&f, "#sum ($a $b) { $a + $b } bind seal; 9 5 sum 7 11 #sum invoke");
  integer(&f, 18);
  integer(&f, 14);
  run(&f, "#capture ($a) { ($b) { $a $b add } lambda } bind seal 12 capture "
      "#add12 swap set 8 add12 2 add12");
  integer(&f, 14);
  integer(&f, 20);
  run(&f, "#outer ($a) { ($a) { $a 2 mul } lambda } bind seal 88 outer 7 swap invoke");
  integer(&f, 14);
  reject(&f, "#sum 14 set", "sealed", 8);
  reject(&f, "clear 1 sum", "too few arguments", 8);
  reject(&f, "clear ($a $a) { $a } lambda", "duplicate parameter", SIZE_MAX);
  reject(&f, "99 { drop true } if { 1 }", "underflow", 5);
  integer(&f, 99);
  reject(&f, "{ 1 } { 2 } if", "expected a boolean", SIZE_MAX);
  clear(&f);
}

static void
test_typed_values(void)
{
  fixture_t f = {0};
  mulib_termscript_vm_init(&f.vm, NULL);
  run(&f, "@vec [ 2 3, \"x\" ] set; @vec 1 9 set-at @vec 1 get @vec length "
      "%rec <x: 1, y: <flag: true>> set; %rec .x 6 set-field %rec .x get "
      "%rec .y field \"flag\" at ( @vec #other `echo hi` ) "
      "#\\nul node eval \"a\\x00\\n\\e\" `k\\`f`");
  text(&f, MULIB_TERMSCRIPT_VALUE_COMMAND, "k`f", 3);
  text(&f, MULIB_TERMSCRIPT_VALUE_STRING, "a\0\n\033", 4);
  mulib_termscript_value_t ch = pop(&f);
  assert(ch.kind == MULIB_TERMSCRIPT_VALUE_CHARACTER && ch.integer == 0 && ch.length == 1);
  mulib_termscript_value_t tuple = pop(&f);
  assert(tuple.kind == MULIB_TERMSCRIPT_VALUE_TUPLE && mulib_termscript_value_count(&tuple) == 3);
  const mulib_termscript_value_t *v = mulib_termscript_value_at(&tuple, 0, NULL, NULL);
  assert(v && v->kind == MULIB_TERMSCRIPT_VALUE_ARRAY_SYMBOL);
  mulib_termscript_value_t array;
  assert(mulib_termscript_resolve(&f.vm, v, &array));
  v = mulib_termscript_value_at(&array, 1, NULL, NULL);
  assert(v && v->kind == MULIB_TERMSCRIPT_VALUE_INTEGER && v->integer == 9);
  boolean(&f, 1);
  integer(&f, 6);
  integer(&f, 3);
  integer(&f, 9);
  reject(&f, "@vec -1 at", "bounds", 8);
  reject(&f, "%rec .unknown at", "unknown record field", 14);
  reject(&f, "<a: 1, a: 2>", "duplicate record field", SIZE_MAX);
  reject(&f, "(1 2) 0 9 set-at", "immutable", SIZE_MAX);
  reject(&f, "@wrong <x: 1> set", "wrong value type", SIZE_MAX);
  assert(mulib_termscript_push_char(&f.vm, 0x1f642));
  text(&f, MULIB_TERMSCRIPT_VALUE_CHARACTER, "\xf0\x9f\x99\x82", 4);
  assert(!mulib_termscript_push_char(&f.vm, 0xd800));
  clear(&f);
}

static void
test_modes(void)
{
  fixture_t f = {0};
  mulib_termscript_error_t err;
  mulib_termscript_vm_init(&f.vm, NULL);
  assert(mulib_termscript_vm_set_term(&f.vm, "xterm-256color", 14));
  size_t term_n;
  assert(!memcmp(mulib_termscript_vm_term(&f.vm, &term_n), "xterm-256color", 14) && term_n == 14);
  run(&f, "%module: sample\n"
      "%if-term: vt100, xterm*\n"
      "  $TERM\n"
      "  %if-term: vt100\n"
      "    unknown\n"
      "  %else:\n"
      "    27\n"
      "  %end:\n"
      "%else:\n"
      "  unknown\n"
      "%end:\n"
      "!F1 { 1 } bind !F2 { \"edit\" mode } bind \"edit\" mode !F1 { 2 } bind \"\" mode");
  integer(&f, 27);
  text(&f, MULIB_TERMSCRIPT_VALUE_STRING, "xterm-256color", 14);
  assert(mulib_termscript_key(&f.vm, "F1", 2, &err) == 1);
  integer(&f, 1);
  assert(mulib_termscript_key(&f.vm, "F2", 2, &err) == 1);
  size_t mode_n;
  assert(!memcmp(mulib_termscript_vm_mode(&f.vm, &mode_n), "edit", 4) && mode_n == 4);
  assert(mulib_termscript_key(&f.vm, "F1", 2, &err) == 1);
  integer(&f, 2);
  run(&f, "!F2 press !F1 press");
  integer(&f, 2);
  reject(&f, "!F3 press", "unbound key", 4);
  text(&f, MULIB_TERMSCRIPT_VALUE_KEY, "F3", 2);
  assert(mulib_termscript_key(&f.vm, "F3", 2, &err) == 0 && !err.message);
  reject(&f, "%end:\n", "unexpected %end", 0);
  reject(&f, "%if-term: vt100\n 1", "missing %end", SIZE_MAX);
  clear(&f);
}

typedef struct {
  unsigned loads, calls, emissions;
  const mulib_termscript_program_t *nested;
} host_t;

static int
load(void *user, const char *name, size_t n, mulib_termscript_program_t *out,
     mulib_termscript_error_t *err)
{
  host_t *host = (host_t *)user;
  ++host->loads;
  const char *source = n == 6 && !memcmp(name, "sample", 6)
    ? "#private global 2 mul set; #combine ($a) { $a private add } bind seal"
    : n == 5 && !memcmp(name, "cycle", 5) ? "\"cycle\" import" : NULL;
  return source && mulib_termscript_parse(source, strlen(source), out, err);
}

static int
host_call(mulib_termscript_vm_t *vm, void *user, const char *name, size_t n,
          mulib_termscript_error_t *err)
{
  host_t *host = (host_t *)user;
  const mulib_termscript_value_t *v = mulib_termscript_peek(vm, 0);
  if (n == 4 && !memcmp(name, "emit", 4)) {
    mulib_termscript_value_t escape;
    if (!v || !mulib_termscript_resolve(vm, v, &escape) || escape.kind != MULIB_TERMSCRIPT_VALUE_ESCAPE)
      return 0;
    if (escape.length == 4 && !memcmp(escape.text, "kf11", 4)) {
      mulib_termscript_pop(vm, NULL);
    } else if (escape.length == 3 && !memcmp(escape.text, "CUP", 3)) {
      const mulib_termscript_value_t *args = mulib_termscript_peek(vm, 1);
      if (!args || args->kind != MULIB_TERMSCRIPT_VALUE_TUPLE
          || mulib_termscript_value_count(args) != 2) return 0;
      const mulib_termscript_value_t *row = mulib_termscript_value_at(args, 0, NULL, NULL);
      const mulib_termscript_value_t *column = mulib_termscript_value_at(args, 1, NULL, NULL);
      assert(row && row->kind == MULIB_TERMSCRIPT_VALUE_INTEGER && row->integer == 10);
      assert(column && column->kind == MULIB_TERMSCRIPT_VALUE_INTEGER && column->integer == 20);
      mulib_termscript_pop(vm, NULL);
      mulib_termscript_pop(vm, NULL);
    } else {
      return 0;
    }
    ++host->emissions;
    return 1;
  }
  if (n == 4 && !memcmp(name, "exec", 4)) {
    const mulib_termscript_value_t *command = mulib_termscript_peek(vm, 1);
    if (!v || !command || v->kind != MULIB_TERMSCRIPT_VALUE_MODULE
        || v->length != 15 || memcmp(v->text, "stdlib.terminfo", 15)
        || command->kind != MULIB_TERMSCRIPT_VALUE_COMMAND) return 0;
    assert(command->length == 4 && !memcmp(command->text, "kf11", 4));
    mulib_termscript_pop(vm, NULL);
    mulib_termscript_pop(vm, NULL);
    ++host->calls;
    return 1;
  }
  if (n == 6 && !memcmp(name, "import", 6)) {
    if (!v || v->kind != MULIB_TERMSCRIPT_VALUE_STRING || v->length != 15
        || memcmp(v->text, "stdlib.terminfo", 15)) return 0;
    mulib_termscript_value_t value = *v;
    value.kind = MULIB_TERMSCRIPT_VALUE_MODULE;
    mulib_termscript_pop(vm, NULL);
    return mulib_termscript_push(vm, &value) ? 1 : -1;
  }
  if (n == 6 && !memcmp(name, "nested", 6))
    return mulib_termscript_eval(vm, host->nested, err) ? 1 : -1;
  return 0;
}

static void
test_host_and_imports(void)
{
  fixture_t f = {0};
  host_t host = {0, 0, 0, NULL};
  mulib_termscript_vm_init(&f.vm, NULL);
  run(&f, "`this literal is inert`");
  text(&f, MULIB_TERMSCRIPT_VALUE_COMMAND, "this literal is inert", 21);
  reject(&f, "`disabled` exec", "not enabled", 11);
  reject(&f, "\"disabled\" import", "not enabled", 11);
  mulib_termscript_vm_set_loader(&f.vm, load, &host);
  mulib_termscript_vm_set_host(&f.vm, host_call, &host);
  run(&f, "clear #global 7 set 4 `combine` \"sample\" import exec \"sample\" use .private at "
      "#Terminfo { \"stdlib.terminfo\" import } bind seal; `kf11` #Terminfo exec;");
  integer(&f, 14);
  integer(&f, 18);
  assert(host.loads == 1 && host.calls == 1);
  reject(&f, "private", "unknown word", 0);
  reject(&f, "\"cycle\" import", "cyclic import", 8);
  assert(host.loads == 2);
  reject(&f, "\"cycle\" import", "initialization failed", 8);
  assert(host.loads == 2);
  host.nested = parse(&f, "9 8 add");
  run(&f, "clear nested");
  integer(&f, 17);
  clear(&f);
}

static void
test_escapes(void)
{
  fixture_t f = {0};
  host_t host = {0, 0, 0, NULL};
  mulib_termscript_vm_init(&f.vm, NULL);
  run(&f, "[[ kf11 ]] [[CUP]] node eval [ [[CUP]] ] 0 at");
  text(&f, MULIB_TERMSCRIPT_VALUE_ESCAPE, "CUP", 3);
  text(&f, MULIB_TERMSCRIPT_VALUE_ESCAPE, "CUP", 3);
  text(&f, MULIB_TERMSCRIPT_VALUE_ESCAPE, "kf11", 4);
  assert(host.emissions == 0);
  reject(&f, "[[kf11]] emit", "not enabled", 9);
  text(&f, MULIB_TERMSCRIPT_VALUE_ESCAPE, "kf11", 4);
  mulib_termscript_vm_set_host(&f.vm, host_call, &host);
  run(&f, "[[kf11]] emit #cup [[CUP]] set (10,20) #cup get emit "
      "#cup get [[CUP]] eq?");
  assert(host.emissions == 2);
  boolean(&f, 1);
  reject(&f, "[[CUP]] emit", "not enabled", 8);
  text(&f, MULIB_TERMSCRIPT_VALUE_ESCAPE, "CUP", 3);
  clear(&f);
}

static void
test_limits(void)
{
  fixture_t f = {0};
  mulib_termscript_limits_t limits = {50, 6, 0, 0};
  mulib_termscript_vm_init(&f.vm, &limits);
  reject(&f, "{ true } while {} loop", "step limit", SIZE_MAX);
  reject(&f, "9223372036854775807 {} repeat", "step limit", SIZE_MAX);
  reject(&f, "#r { r } bind seal r", "call limit", SIZE_MAX);
  reject(&f, "!again { !again press } bind !again press", "call limit", SIZE_MAX);
  run(&f, "6 { 1 drop } repeat 20");
  integer(&f, 20);
  host_t host = {0, 0, 0, NULL};
  host.nested = parse(&f, "50 {} repeat");
  mulib_termscript_vm_set_host(&f.vm, host_call, &host);
  reject(&f, "nested nested", "step limit", SIZE_MAX);
  clear(&f);

  limits.max_stack = 2;
  mulib_termscript_vm_init(&f.vm, &limits);
  reject(&f, "1 2 over", "stack", SIZE_MAX);
  integer(&f, 2);
  integer(&f, 1);
  clear(&f);

  limits.max_alloc = 1;
  mulib_termscript_vm_init(&f.vm, &limits);
  reject(&f, "1", "allocation limit", 0);
  clear(&f);
}

int
main(void)
{
  test_stack_and_arithmetic();
  test_language();
  test_typed_values();
  test_modes();
  test_host_and_imports();
  test_escapes();
  test_limits();
  return 0;
}
