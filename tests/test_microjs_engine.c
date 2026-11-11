#include <stdint.h>
#include <stdlib.h>
static size_t allocation_budget = SIZE_MAX;
static int allow_allocation(void) {
  if (allocation_budget == SIZE_MAX) return 1;
  if (!allocation_budget) return 0;
  --allocation_budget; return 1;
}
static void *checked_malloc(size_t n) { return allow_allocation() ? malloc(n) : NULL; }
static void *checked_calloc(size_t n, size_t s) { return allow_allocation() ? calloc(n, s) : NULL; }
#define malloc checked_malloc
#define calloc checked_calloc
#define MULIB_IMPL_MINIVM
#define MULIB_IMPL_MICROJS
#include "mulib-microjs.h"
#undef malloc
#undef calloc
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static mulib_microjs_value_t run(mulib_microjs_t *v, const char *source) {
  mulib_microjs_value_t out = {0}; mulib_microjs_error_t error;
  mulib_microjs_status_t status = mulib_microjs_eval(v, source, strlen(source), &out, &error);
  if (status) fprintf(stderr, "microjs %d at %zu:%zu: %s\n%s\n", status, error.line, error.column, error.message, source);
  assert(status == MULIB_MICROJS_OK && !error.code); return out;
}
static void number(mulib_microjs_t *v, const char *source, double expected) {
  mulib_microjs_value_t r = run(v, source);
  assert(r.type == MULIB_MICROJS_NUMBER);
  if (isnan(expected)) assert(isnan(r.number)); else assert(r.number == expected);
  mulib_microjs_value_free(&r);
}
static void boolean(mulib_microjs_t *v, const char *source, int expected) {
  mulib_microjs_value_t r = run(v, source);
  assert(r.type == MULIB_MICROJS_BOOLEAN && r.boolean == expected); mulib_microjs_value_free(&r);
}
static void string(mulib_microjs_t *v, const char *source, const char *expected, size_t length) {
  mulib_microjs_value_t r = run(v, source);
  assert(r.type == MULIB_MICROJS_STRING && r.length == length && !memcmp(r.string, expected, length));
  mulib_microjs_value_free(&r);
}
static void fails(mulib_microjs_t *v, const char *source, mulib_microjs_status_t expected) {
  mulib_microjs_value_t r = {0}; mulib_microjs_error_t e;
  mulib_microjs_status_t status = mulib_microjs_eval(v, source, strlen(source), &r, &e);
  if (status != expected) fprintf(stderr, "expected %d, got %d: %s\n%s\n", expected, status, e.message ? e.message : "success", source);
  assert(status == expected && e.code == status && e.message && e.line && e.column);
  assert(!r.string && !r.object && r.type == MULIB_MICROJS_NULL);
}
static mulib_microjs_status_t host_sum(mulib_microjs_t *v,
    const mulib_microjs_value_t *args, size_t count, mulib_microjs_value_t *out, void *user) {
  size_t i; mulib_microjs_value_t temp = {0}; mulib_microjs_error_t e;
  assert(mulib_microjs_eval(v, "1", 1, &temp, &e) == MULIB_MICROJS_INVALID);
  assert(mulib_microjs_collect(v) != 0);
  out->type = MULIB_MICROJS_NUMBER; out->number = *(double *)user;
  for (i = 0; i < count; ++i) {
    if (args[i].type != MULIB_MICROJS_NUMBER) return MULIB_MICROJS_TYPE;
    out->number += args[i].number;
  }
  return MULIB_MICROJS_OK;
}
static void primitives(mulib_microjs_t *v) {
  mulib_microjs_value_t r = run(v, ""); assert(r.type == MULIB_MICROJS_UNDEFINED); mulib_microjs_value_free(&r);
  number(v, "1 / 0", INFINITY); number(v, "1 / -0", -INFINITY);
  number(v, "0 / 0", NAN); number(v, "5 % 0", NAN); number(v, "1e999", INFINITY);
  boolean(v, "NaN === NaN", 0); boolean(v, "null == undefined", 1);
  boolean(v, "null === undefined", 0); boolean(v, "'2' == 2 && true == 1", 1);
  boolean(v, "'20' < '3' && !NaN && !'' && !undefined", 1);
  number(v, "'3' * 2 + true + null", 7);
  string(v, "'a' + 1 + false", "a1false", 7);
  string(v, "0 || 'fallback'", "fallback", 8);
  string(v, "'' && missing", "", 0);
}
static void language(mulib_microjs_t *v) {
  number(v, "null ?? 42", 42); number(v, "0 ?? missing", 0);
  number(v, "false ? missing : 3", 3);
  boolean(v, "typeof undeclared === 'undefined' && typeof null === 'object'", 1);
  number(v, "0xff + 0b10 + 0o10 + .5 + 1.", 266.5);
  number(v, "1_000 + 2 ** 3 ** 2", 1512);
  number(v, "(-1 >>> 0) + (1 << 31)", 2147483647);
  number(v, "let x=1; x + (x=3)", 4);
  number(v, "x=1; x += (x=4); x", 5);
  number(v, "x=1; x++ + ++x", 4);
  number(v, "let outer=2; {let outer=3; outer=4;} outer", 2);
  number(v, "let hoisted = answer(); function answer(){ return 42; } hoisted", 42);
  number(v, "function fib(n){if(n<2)return n; return fib(n-1)+fib(n-2);} fib(8)", 21);
  number(v, "function maker(x){ return function(y){x+=y; return x;};} let add=maker(10); add(2); add(3)", 15);
  number(v, "add(5)", 20);
  number(v, "function shadow(){var y=4; {var y=5;} return y;} shadow()", 5);
  boolean(v, "function early(){return y; var y=5;} early() === undefined", 1);
  number(v, "let total=0; for(let i=0;i<10;i++){if(i===2)continue; if(i===8)break; total+=i;} total", 26);
  number(v, "let w=0; while(w<3){w++;} w", 3);
  number(v, "let list=[]; for(let i=0;i<3;i++){ list[i]=function(){return i;}; } list[0]()+list[1]()*10+list[2]()*100", 210);
  number(v, "let object={x:4, method:function(y){return this.x+y;}}; object.method(5)", 9);
  boolean(v, "let alias=object; alias===object && {}!=={}", 1);
  number(v, "let array=[1,,3]; array[5]=9; array.length", 6);
  boolean(v, "array[1] === undefined && array[2] === 3", 1);
  boolean(v, "array.length=2; array[5] === undefined && array.length===2", 1);
  string(v, "'\\u0041\\x42\\u{1F600}'", "AB\xf0\x9f\x98\x80", 6);
  number(v, "'😀'.length", 2);
  boolean(v, "'😀' === '\\ud83d\\ude00'", 1);
  string(v, "'😀'[0]", "\xed\xa0\xbd", 3);
  string(v, "'a\\0b'", "a\0b", 3);
  string(v, "'a\\\r\nb'", "ab", 2);
  number(v, "/* block */ let asi=1\nasi+=2\nasi", 3);
  boolean(v, "function newline(){return\n99;} newline()===undefined", 1);
  boolean(v, "Object.is(-0, -0) && !Object.is(-0, 0) && Object.is(NaN, NaN)", 1);
  number(v, "Number(' 0xff ') + Math.max(2,8,4) + Math.floor(1.9)", 264);
  boolean(v, "Array.isArray(Array(3)) && !Array.isArray({}) && Number.isNaN(NaN) && !Number.isNaN('x')", 1);
  boolean(v, "Math.min(0,-0) === 0 && Object.is(Math.round(-0.1),-0)", 1);
  fails(v, "x=90; if(false){let d; let d;}", MULIB_MICROJS_SYNTAX); number(v, "x", 3);
  fails(v, "x=90; for(let d=0,d=1;false;){}", MULIB_MICROJS_SYNTAX); number(v, "x", 3);
  fails(v, "x=90; const bad;", MULIB_MICROJS_SYNTAX); number(v, "x", 3);
  fails(v, "x=90; 1+", MULIB_MICROJS_SYNTAX); number(v, "x", 3);
  fails(v, "{before; let before=1;}", MULIB_MICROJS_NAME);
  fails(v, "{typeof before; let before=1;}", MULIB_MICROJS_NAME);
  fails(v, "{const frozen=1; frozen=2;}", MULIB_MICROJS_TYPE);
  fails(v, "let x=1", MULIB_MICROJS_SYNTAX);
  fails(v, "if(false)let z=1", MULIB_MICROJS_SYNTAX);
  fails(v, "{let conflict; {var conflict;}}", MULIB_MICROJS_SYNTAX);
  fails(v, "function f(a){let a;}", MULIB_MICROJS_SYNTAX);
  fails(v, "1 ?? 2 || 3", MULIB_MICROJS_SYNTAX);
  fails(v, "-2 ** 2", MULIB_MICROJS_SYNTAX);
  fails(v, "throw\n1", MULIB_MICROJS_SYNTAX);
  fails(v, "return 1", MULIB_MICROJS_SYNTAX);
  fails(v, "x=7; throw 'failed'", MULIB_MICROJS_EXCEPTION); number(v, "x", 7);
  fails(v, "function recurse(){return recurse();} recurse()", MULIB_MICROJS_LIMIT);
}
static void embedding(mulib_microjs_t *v) {
  mulib_microjs_value_t f = {0}, arg = {0}, out = {0}, object = {0};
  mulib_microjs_error_t e; double base = 10; size_t before, after, i;
  assert(mulib_microjs_bind(v, "hostSum", host_sum, &base) == MULIB_MICROJS_OK);
  number(v, "hostSum(2,3)", 15); fails(v, "hostSum('bad')", MULIB_MICROJS_TYPE);
  arg.type = MULIB_MICROJS_NUMBER; arg.number = 5;
  assert(mulib_microjs_set_global(v, "fromC", &arg) == MULIB_MICROJS_OK);
  number(v, "fromC * 2", 10);
  assert(mulib_microjs_get_global(v, "add", &f) == MULIB_MICROJS_OK);
  assert(mulib_microjs_call(v, &f, &arg, 1, &out, &e) == MULIB_MICROJS_OK && out.number == 25);
  mulib_microjs_value_free(&out); mulib_microjs_value_free(&f);
  object = run(v, "({answer: 42})");
  assert(mulib_microjs_get_property(v, &object, "answer", &out) == MULIB_MICROJS_OK && out.number == 42);
  mulib_microjs_value_free(&out);
  assert(mulib_microjs_set_property(v, &object, "answer", &arg) == MULIB_MICROJS_OK);
  assert(mulib_microjs_get_property(v, &object, "answer", &out) == MULIB_MICROJS_OK && out.number == 5);
  mulib_microjs_value_free(&out); mulib_microjs_value_free(&object);
  assert(mulib_microjs_make_object(v, 1, &object) == MULIB_MICROJS_OK);
  assert(mulib_microjs_set_property(v, &object, "0", &arg) == MULIB_MICROJS_OK);
  assert(mulib_microjs_value_copy(v, &object, &out) == MULIB_MICROJS_OK);
  mulib_microjs_value_free(&object);
  assert(mulib_microjs_get_property(v, &out, "length", &object) == MULIB_MICROJS_OK && object.number == 1);
  mulib_microjs_value_free(&out); mulib_microjs_value_free(&object);
  before = mulib_microjs_collect(v);
  for (i = 0; i < 20; ++i) {
    out = run(v, "(function(){let cycle={};cycle.self=cycle;return cycle;})()");
    mulib_microjs_value_free(&out);
  }
  after = mulib_microjs_collect(v); assert(before == after);
  out = run(v, "'owned'"); object = run(v, "({kept:1})");
  mulib_microjs_destroy(v);
  assert(!strcmp(out.string, "owned")); mulib_microjs_value_free(&out); mulib_microjs_value_free(&object);
}
static void limits(void) {
  mulib_microjs_limits_t l = mulib_microjs_default_limits(); mulib_microjs_t *v;
  l.steps = 1000; v = mulib_microjs_create(&l); assert(v);
  fails(v, "while(true){}", MULIB_MICROJS_LIMIT); number(v, "1", 1); mulib_microjs_destroy(v);
  v = mulib_microjs_create_with_heap(NULL, 2048); assert(v);
  fails(v, "let object={a:1,b:2,c:3}", MULIB_MICROJS_LIMIT); mulib_microjs_destroy(v);
  l = mulib_microjs_default_limits(); l.source_bytes = 0; v = mulib_microjs_create(&l); assert(v);
  fails(v, "1", MULIB_MICROJS_LIMIT); mulib_microjs_destroy(v);
  assert(!mulib_microjs_create_with_heap(NULL, 0));
}
static void allocation_failures(void) {
  const char source[] = "function factory(x){return function(y){return {n:x+y,s:'kept'};};} factory(2)(3)";
  size_t budget; int success = 0;
  for (budget = 0; budget < 1024; ++budget) {
    mulib_microjs_t *v; mulib_microjs_value_t out = {0}; mulib_microjs_error_t error;
    allocation_budget = budget; v = mulib_microjs_create(NULL);
    if (v) {
      mulib_microjs_status_t rc = mulib_microjs_eval(v, source, sizeof source - 1, &out, &error);
      assert(rc == MULIB_MICROJS_OK || rc == MULIB_MICROJS_MEMORY);
      success = rc == MULIB_MICROJS_OK;
      mulib_microjs_destroy(v); mulib_microjs_value_free(&out);
    }
    if (success) break;
  }
  allocation_budget = SIZE_MAX; assert(success);
}
static void malformed_programs(void) {
  const char source[] = "let a={x:[1,2],f:function(n){return n+1;}}; for(let i=0;i<2;i++){a.x[i]=a.f(i);} a.x[1]";
  char bytes[sizeof source]; size_t i, n; uint32_t random = 12345;
  mulib_microjs_limits_t limits = mulib_microjs_default_limits();
  limits.steps = 2000; limits.variables = 128;
  for (i = 0; i < 1024; ++i) {
    mulib_microjs_value_t out = {0}; mulib_microjs_error_t error;
    mulib_microjs_t *v = mulib_microjs_create(&limits); mulib_microjs_status_t rc;
    assert(v); memcpy(bytes, source, sizeof source);
    if (i < sizeof source) n = i;
    else {
      n = sizeof source - 1; random = random * 1664525u + 1013904223u;
      bytes[random % n] = (char)(random >> 24);
    }
    rc = mulib_microjs_eval(v, bytes, n, &out, &error);
    if (rc != MULIB_MICROJS_OK) {
      assert(error.code == rc && error.offset <= n && error.line && error.column);
      assert(out.type == MULIB_MICROJS_NULL && !out.string && !out.object);
    }
    mulib_microjs_value_free(&out); mulib_microjs_destroy(v);
  }
}
int main(void) {
  mulib_microjs_t *v = mulib_microjs_create(NULL); assert(v);
  primitives(v); language(v); embedding(v); limits(); allocation_failures(); malformed_programs();
  puts("microjs engine tests passed"); return 0;
}
