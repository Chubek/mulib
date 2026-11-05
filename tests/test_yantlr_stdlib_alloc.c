#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t allocations, fail_at, live;
static void *checked_malloc(size_t n) {
  void *p;
  if (++allocations == fail_at) return NULL;
  p = malloc(n); if (p) ++live; return p;
}
static void *checked_calloc(size_t n, size_t size) {
  void *p;
  assert(!size || n <= SIZE_MAX / size);
  p = checked_malloc(n * size); if (p) memset(p, 0, n * size); return p;
}
static void *checked_realloc(void *old, size_t n) {
  void *p;
  int existed = old != NULL;
  assert(n);
  if (++allocations == fail_at) return NULL;
  p = realloc(old, n); if (p && !existed) ++live; return p;
}
static void checked_free(void *p) { if (p) { assert(live); --live; } free(p); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_YANTLR
#define MULIB_IMPL_YANTLR_STDLIB
#include "../library/yantlr/mulib-yantlr-stdlib.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

int main(void) {
  const char *source = "intermediary \"allocation\";"
    "set G Services[Parse]::PEG!\"document <- word+; word <- 'a';\";"
    "set Match Services[Parse]::Run G \"aaa\";"
    "set Encoded Services[JSON]::Stringify!{\"a\\u0000b\":[null,true,1,\"hello\"]};"
    "set Decoded Services[JSON]::Parse Encoded;"
    "set Mapped Services[Array]::Map (\\x -> x `add` 1) (Services[Array]::Range!3);"
    "set Filtered Services[Array]::Filter (\\x -> x `gt` 1) Mapped;"
    "set Sum Services[Array]::Fold add 0 Filtered;"
    "set Keys Services[Record]::Keys Decoded;"
    "set Joined Services[String]::Join \",\" Keys;"
    "set Content Services[String]::Concat Joined Encoded;"
    "set Output Services[IO]::Emit \"report\" Content;";
  mulib_yantlr_stdlib_t *library;
  mulib_yantlr_module_t *module;
  mulib_yantlr_run_t *run;
  mulib_yantlr_error_t error;
  const mulib_yantlr_binding_t *bindings;
  size_t count, i, baseline, binding_count;
  library = mulib_yantlr_stdlib_create(NULL); assert(library);
  count = allocations; mulib_yantlr_stdlib_destroy(library); assert(!live);
  for (i = 1; i <= count; ++i) {
    allocations = 0; fail_at = i;
    assert(!mulib_yantlr_stdlib_create(NULL)); assert(!live);
  }
  fail_at = 0;
  library = mulib_yantlr_stdlib_create(NULL); assert(library);
  bindings = mulib_yantlr_stdlib_bindings(library, &binding_count);
  module = mulib_yantlr_parse(source, strlen(source), NULL, &error); assert(module);
  baseline = live; allocations = 0;
  run = mulib_yantlr_evaluate(module, bindings, binding_count, NULL, &error);
  assert(run); count = allocations; mulib_yantlr_run_destroy(run); assert(live == baseline);
  for (i = 1; i <= count; ++i) {
    /* MuLIB JSON combines numeric range and allocation failures in one
     * diagnostic; like the host parser, the provider classifies it as syntax. */
    int expected_error;
    allocations = 0; fail_at = i;
    run = mulib_yantlr_evaluate(module, bindings, binding_count, NULL, &error);
    expected_error = error.code == MULIB_YANTLR_MEMORY ||
        (error.code == MULIB_YANTLR_SYNTAX && strstr(error.message, "range or out of memory"));
    if (run || !expected_error)
      fprintf(stderr, "allocation %zu/%zu: code %d: %s\n", i, count, (int)error.code, error.message);
    assert(!run && expected_error && *error.message);
    assert(live == baseline);
  }
  mulib_yantlr_module_destroy(module); mulib_yantlr_stdlib_destroy(library); assert(!live);
  return 0;
}
