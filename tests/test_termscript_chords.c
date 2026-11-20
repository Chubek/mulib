#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_TERMSCRIPT_AST
#define MULIB_IMPL_TERMSCRIPT_RUNTIME
#include "mulib-termscript-runtime.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  mulib_termscript_vm_t vm;
  mulib_termscript_program_t programs[16];
  size_t count;
} fixture_t;

static const mulib_termscript_program_t *
parse(fixture_t *f, const char *source)
{
  mulib_termscript_error_t err;
  assert(f->count < sizeof f->programs / sizeof *f->programs);
  mulib_termscript_program_t *p = &f->programs[f->count++];
  assert(mulib_termscript_parse(source, strlen(source), p, &err));
  assert(!err.message);
  return p;
}

static void
run(fixture_t *f, const char *source)
{
  mulib_termscript_error_t err;
  if (!mulib_termscript_run(&f->vm, parse(f, source), &err)) {
    fprintf(stderr, "%zu: %s: %s\n", err.offset, err.message, source);
    assert(0);
  }
  assert(!err.message);
}

static void
clear(fixture_t *f)
{
  mulib_termscript_vm_clear(&f->vm);
  for (size_t i = 0; i < f->count; ++i) mulib_termscript_program_free(&f->programs[i]);
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

static int
host(mulib_termscript_vm_t *vm, void *user, const char *word, size_t n,
     mulib_termscript_error_t *err)
{
  (void)err;
  unsigned *emissions = (unsigned *)user;
  const mulib_termscript_value_t *v = mulib_termscript_peek(vm, 0);
  if (n != 4 || memcmp(word, "emit", n) || !v || v->kind != MULIB_TERMSCRIPT_VALUE_ESCAPE)
    return 0;
  ++*emissions;
  assert(mulib_termscript_pop(vm, NULL));
  return 1;
}

static void
test_values(void)
{
  fixture_t f = {0};
  mulib_termscript_error_t err;
  unsigned emissions = 0;
  char source[] = "[[ F1 ]] [[ z,\t!SuPeR, Shift,\nALT, b, !CTRL, f_1, A-key ]]";
  const mulib_termscript_program_t *p = parse(&f, source);
  memset(source, 'x', sizeof source);
  const mulib_termscript_node_t *escape = &p->nodes[p->root];
  const mulib_termscript_node_t *chord = &p->nodes[escape->next];
  assert(p->count == 2);
  assert(escape->kind == MULIB_TERMSCRIPT_ESCAPE && escape->offset == 3 && escape->length == 2);
  assert(escape->first == SIZE_MAX);
  assert(chord->kind == MULIB_TERMSCRIPT_CHORD && chord->offset == 9);
  assert(chord->first == SIZE_MAX && chord->next == SIZE_MAX);
  assert(chord->offset + chord->length == p->length);
  assert(!memcmp(p->source + chord->offset, "[[", 2));
  assert(!memcmp(p->source + p->length - 2, "]]", 2));

  mulib_termscript_vm_init(&f.vm, NULL);
  mulib_termscript_vm_set_host(&f.vm, host, &emissions);
  assert(mulib_termscript_run(&f.vm, p, &err));
  assert(mulib_termscript_stack_size(&f.vm) == 2 && emissions == 0);
  const char canon[] = "ctrl+alt+shift+super+z+b+f_1+A-key";
  mulib_termscript_value_t v = pop(&f);
  assert(v.kind == MULIB_TERMSCRIPT_VALUE_KEY && v.program == p && v.node == escape->next);
  assert(v.length == sizeof canon - 1 && !memcmp(v.text, canon, v.length));
  v = pop(&f);
  assert(v.kind == MULIB_TERMSCRIPT_VALUE_ESCAPE && v.length == 2 && !memcmp(v.text, "F1", 2));

  run(&f, "[[ !F1 ]] [[ F1, ]] node eval eq? [[a,A]] !a+A eq? [[F1]] !F1 ne?");
  for (unsigned i = 0; i < 3; ++i) {
    v = pop(&f);
    assert(v.kind == MULIB_TERMSCRIPT_VALUE_BOOL && v.integer == 1);
  }
  assert(emissions == 0);
  run(&f, "[[F1]] emit");
  assert(emissions == 1 && !mulib_termscript_stack_size(&f.vm));
  assert(!mulib_termscript_run(&f.vm, parse(&f, "[[F1,]] emit"), &err));
  assert(err.message && strstr(err.message, "emission") && emissions == 1);
  assert(pop(&f).kind == MULIB_TERMSCRIPT_VALUE_KEY);
  assert(!mulib_termscript_run(&f.vm, parse(&f, "[[F1]] press"), &err));
  assert(err.message && strstr(err.message, "expected a key"));
  assert(pop(&f).kind == MULIB_TERMSCRIPT_VALUE_ESCAPE);
  clear(&f);
}

static void
test_dispatch(void)
{
  fixture_t f = {0};
  mulib_termscript_error_t err;
  mulib_termscript_vm_init(&f.vm, NULL);
  run(&f, "[[alt, b, ctrl, A]] { 10 } bind "
          "[[ctrl, alt, A, b]] { 20 } bind-key "
          "[[F1,]] { 30 } bind");
  assert(!mulib_termscript_stack_size(&f.vm));
  run(&f, "[[ctrl, alt, b, A]]");
  assert(mulib_termscript_stack_size(&f.vm) == 1);
  assert(pop(&f).kind == MULIB_TERMSCRIPT_VALUE_KEY);
  run(&f, "[[b, CTRL, Alt, A]] press [[A, alt, ctrl, b]] press !F1 press");
  integer(&f, 30);
  integer(&f, 20);
  integer(&f, 10);
  const char event[] = { 'c', 't', 'r', 'l', '+', 'a', 'l', 't', '+', 'b', '+', 'A' };
  assert(mulib_termscript_key(&f.vm, event, sizeof event, &err) == 1 && !err.message);
  integer(&f, 10);
  assert(mulib_termscript_key(&f.vm, "alt+ctrl+b+A", sizeof event, &err) == 0 && !err.message);
  assert(mulib_termscript_key(&f.vm, "ctrl+alt+b+a", sizeof event, &err) == 0 && !err.message);
  assert(!mulib_termscript_run(&f.vm, parse(&f, "[[ctrl, alt, x]] press"), &err));
  assert(err.message && strstr(err.message, "unbound key"));
  mulib_termscript_value_t v = pop(&f);
  assert(v.kind == MULIB_TERMSCRIPT_VALUE_KEY && v.length == 10 && !memcmp(v.text, "ctrl+alt+x", 10));

  run(&f, "\"edit\" mode !ctrl+alt+b+A { 40 } bind");
  assert(mulib_termscript_key(&f.vm, event, sizeof event, &err) == 1);
  integer(&f, 40);
  run(&f, "[[b, alt, ctrl, A]] press [[!F1]] press");
  integer(&f, 30);
  integer(&f, 40);
  run(&f, "{ 50 } [[b, ctrl, alt, A]] bind !ctrl+alt+b+A press");
  integer(&f, 50);
  run(&f, "\"\" mode");
  assert(mulib_termscript_key(&f.vm, event, sizeof event, &err) == 1);
  integer(&f, 10);
  assert(!mulib_termscript_stack_size(&f.vm));
  clear(&f);
}

static void
reject(const char *source, const char *diagnostic, size_t offset)
{
  mulib_termscript_program_t p = {0};
  mulib_termscript_error_t err;
  p.root = 42;
  assert(!mulib_termscript_parse(source, strlen(source), &p, &err));
  if (!err.message || !strstr(err.message, diagnostic) || err.offset != offset) {
    fprintf(stderr, "parse %.40s: expected %zu: %s; got %zu: %s\n",
            source, offset, diagnostic, err.offset, err.message ? err.message : "(no error)");
    assert(0);
  }
  assert(p.root == 42 && !p.source && !p.nodes);
}

static void
test_invalid_and_limits(void)
{
  reject("[[,a]]", "escape name", 2);
  reject("[[a,,b]]", "chord key name", 4);
  reject("[[a,b,]]", "chord key name", 6);
  reject("[[a, ! ]]", "chord key name", 6);
  reject("[[a b]]", "']]'", 4);
  reject("[[a, b c]]", "expected ','", 7);
  reject("[[a, b+c]]", "expected ','", 6);
  reject("[[a, x; b]]", "expected ','", 6);
  reject("[[a, \"b\"]]", "chord key name", 5);
  reject("[[a, \xc3\xa9]]", "chord key name", 5);
  reject("[[a, b] ]", "expected ','", 6);
  reject("1 [[a, b]", "']]'", 8);
  reject("[[!a, a]]", "duplicate", 6);
  reject("[[Ctrl, !cTrL, a]]", "duplicate", 9);
  reject("[[a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q]]", "component limit", 34);

  /* All sixteen names at their 63-byte limit use every canonical key byte. */
  char source[4100];
  size_t pos = 0;
  source[pos++] = '[';
  source[pos++] = '[';
  for (size_t i = 0; i < 16; ++i) {
    if (i) source[pos++] = ',';
    memset(source + pos, (int)('a' + i), 63);
    pos += 63;
  }
  source[pos++] = ']';
  source[pos++] = ']';
  source[pos] = 0;
  fixture_t f = {0};
  mulib_termscript_vm_init(&f.vm, NULL);
  run(&f, source);
  mulib_termscript_value_t v = pop(&f);
  assert(v.kind == MULIB_TERMSCRIPT_VALUE_KEY && v.length == 1023);
  for (size_t i = 0; i < v.length; ++i)
    assert(v.text[i] == (i % 64 == 63 ? '+' : (int)('a' + i / 64)));
  clear(&f);

  source[3] = ',';
  memset(source + 4, 'A', 64);
  strcpy(source + 68, "]]");
  reject(source, "chord name limit", 67);
  memset(source + 4, ' ', 4089);
  strcpy(source + 4093, "b]]");
  mulib_termscript_program_t p = {0};
  assert(mulib_termscript_parse(source, 4096, &p, NULL));
  assert(p.count == 1 && p.nodes[p.root].length == 4096);
  mulib_termscript_program_free(&p);
  memset(source + 4, ' ', 4092);
  strcpy(source + 4096, "]]");
  reject(source, "chord source limit", 4096);
}

static void
test_runtime_errors(void)
{
  fixture_t f = {0};
  mulib_termscript_error_t err;
  mulib_termscript_limits_t limits = { 32, 0, 0, 0 };
  mulib_termscript_vm_init(&f.vm, &limits);
  char raw[] = "[[a, b]]x";
  mulib_termscript_node_t node = {
    MULIB_TERMSCRIPT_CHORD, 0, sizeof raw - 1, SIZE_MAX, SIZE_MAX, 0
  };
  mulib_termscript_program_t ast = { raw, sizeof raw - 1, &node, 1, 0 };
  assert(mulib_termscript_push_int(&f.vm, 91));
  assert(!mulib_termscript_run(&f.vm, &ast, &err));
  assert(err.message && strstr(err.message, "invalid chord AST"));
  --node.length;
  node.first = 0;
  assert(!mulib_termscript_run(&f.vm, &ast, &err));
  assert(err.message && strstr(err.message, "invalid chord AST"));
  node.first = SIZE_MAX;
  raw[0] = 'a';
  assert(!mulib_termscript_run(&f.vm, &ast, &err));
  assert(err.message && strstr(err.message, "invalid chord AST"));
  /* Foreign ASTs need not supply a sentinel after their advertised source.
   * ASan checks every truncated prefix against its exact allocation. */
  const char complete[] = "[[a, !ctrl]]";
  for (size_t len = 0; len < sizeof complete - 1; ++len) {
    char *truncated = (char *)malloc(len ? len : 1);
    assert(truncated);
    memcpy(truncated, complete, len);
    ast.source = truncated;
    ast.length = node.length = len;
    assert(!mulib_termscript_run(&f.vm, &ast, &err));
    assert(err.message && strstr(err.message, "invalid chord AST"));
    free(truncated);
  }
  assert(mulib_termscript_stack_size(&f.vm) == 1);
  integer(&f, 91);

  const mulib_termscript_program_t *literal = parse(&f, "[[ctrl,a]]");
  const size_t budget = f.vm.limits.max_alloc;
  f.vm.limits.max_alloc = f.vm.state->used; /* Stack allocated, no space for a chord name. */
  assert(!mulib_termscript_run(&f.vm, literal, &err));
  assert(err.message && strstr(err.message, "allocation") && !mulib_termscript_stack_size(&f.vm));
  f.vm.limits.max_alloc = budget;
  assert(mulib_termscript_run(&f.vm, literal, &err));
  assert(pop(&f).kind == MULIB_TERMSCRIPT_VALUE_KEY);
  run(&f, "[[ctrl, a]] { [[a, ctrl]] press } bind [[alt, a]] { 55 } bind");
  assert(mulib_termscript_key(&f.vm, "ctrl+a", 6, &err) == -1);
  assert(err.message && strstr(err.message, "step limit"));
  assert(mulib_termscript_key(&f.vm, "alt+a", 5, &err) == 1 && !err.message);
  integer(&f, 55);
  clear(&f);
}

int
main(void)
{
  test_values();
  test_dispatch();
  test_invalid_and_limits();
  test_runtime_errors();
  return 0;
}
