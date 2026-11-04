#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t allocations, fail_at, live, releases;
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
#include "mulib-yantlr.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void release(void *p) { ++releases; checked_free(p); }
static int provider(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *const *a,
    size_t n, const mulib_yantlr_value_t **out, void *user) {
  char bytes[12000];
  void *p = checked_malloc(1);
  (void)a; (void)n; (void)user;
  if (!p) return mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "test allocation");
  *out = mulib_yantlr_opaque(r, "test.owner", p, release);
  if (!*out) { checked_free(p); return 0; }
  memset(bytes, 'a', sizeof bytes);
  return mulib_yantlr_emit(r, "generated.c", bytes, sizeof bytes);
}
int main(void) {
  const char *source = "intermediary \"alloc\"; set Lambda \\x -> x;"
      "set Data {\"name\":\"\\ud83d\\ude00\",\"array\":[1,2,true,null]};"
      "set Shared Config;"
      "set Build Host[Core]::Make!(Lambda Data);";
  mulib_yantlr_binding_t bindings[] = {{"Host[Core]::Make", 1, provider, NULL, NULL}, {"Config", 0, NULL, NULL, NULL}};
  mulib_json_t *constant;
  mulib_yantlr_module_t *m;
  mulib_yantlr_run_t *r;
  mulib_yantlr_error_t error;
  size_t count, i, baseline;
  m = mulib_yantlr_parse(source, strlen(source), NULL, &error); assert(m);
  count = allocations;
  mulib_yantlr_module_destroy(m); assert(!live);
  for (i = 1; i <= count; ++i) {
    allocations = 0; fail_at = i;
    m = mulib_yantlr_parse(source, strlen(source), NULL, &error);
    assert(!m && error.code != MULIB_YANTLR_OK && *error.message);
    assert(!live);
  }
  fail_at = 0;
  m = mulib_yantlr_parse(source, strlen(source), NULL, &error); assert(m);
  constant = mulib_json_parse("{\"outer\":[true,3,\"value\"]}", 26, NULL); assert(constant);
  bindings[1].constant = constant;
  baseline = live; allocations = 0;
  r = mulib_yantlr_evaluate(m, bindings, 2, NULL, &error); assert(r);
  count = allocations; mulib_yantlr_run_destroy(r); assert(live == baseline && releases == 1);
  for (i = 1; i <= count; ++i) {
    allocations = 0; fail_at = i;
    r = mulib_yantlr_evaluate(m, bindings, 2, NULL, &error);
    assert(!r && error.code == MULIB_YANTLR_MEMORY && *error.message);
    assert(live == baseline);
  }
  mulib_json_destroy(constant); mulib_yantlr_module_destroy(m); assert(!live);
  return 0;
}
