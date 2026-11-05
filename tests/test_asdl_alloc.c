/* Fail every allocation site in turn; no dependency allocation is exempt. */
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
static size_t calls, fail_at = (size_t)-1, live;
static void *checked_malloc(size_t n) {
  void *p;
  if (calls++ == fail_at) return NULL;
  p = malloc(n); if (p) ++live; return p;
}
static void *checked_calloc(size_t n, size_t s) {
  void *p;
  if (calls++ == fail_at) return NULL;
  p = calloc(n, s); if (p) ++live; return p;
}
static void *checked_realloc(void *old, size_t n) {
  void *p;
  int fresh = old == NULL;
  if (calls++ == fail_at) return NULL;
  p = realloc(old, n); if (p && fresh) ++live; return p;
}
static void checked_free(void *p) { if (p) { assert(live); --live; } free(p); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_ASDL
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#include "mulib-asdl.h"

static size_t exercise(int operation) {
  static const char source[] = "module Test { pos = (int line) expr = Num(int value) | Add(expr, expr) | Empty attributes (pos? p) }";
  static const mulib_asdl_field_t fields[] = {MULIB_ASDL_FIELD("int", "value")};
  static const mulib_asdl_constructor_t ctors[] = {MULIB_ASDL_CONSTRUCTOR("Num", fields)};
  static const mulib_asdl_type_t types[] = {MULIB_ASDL_SUM_TYPE("expr", ctors)};
  mulib_asdl_module_t *m;
  mulib_asdl_error_t e;
  mulib_asdl_options_t o = {0};
  mulib_strbuf_t output = {0};
  size_t count;
  int ok;
  calls = 0;
  if (!operation) {
    m = mulib_asdl_parse(source, sizeof source - 1, NULL, &e);
    if (!m) assert(e.code == MULIB_ASDL_MEMORY);
    mulib_asdl_destroy(m);
  } else if (operation == 1) {
    m = mulib_asdl_from_types("Test", types, 1, &e);
    if (!m) assert(e.code == MULIB_ASDL_MEMORY);
    mulib_asdl_destroy(m);
  } else {
    o.format = (mulib_asdl_format_t)(operation - 2);
    ok = mulib_asdl_compile(source, sizeof source - 1, &o, &output, &e);
    if (!ok) assert(e.code == MULIB_ASDL_MEMORY && !output.length && !output.data);
    mulib_strbuf_free(&output);
  }
  count = calls;
  assert(!live); return count;
}
int main(void) {
  size_t i, count;
  int operation;
  for (operation = 0; operation < 5; ++operation) {
    fail_at = SIZE_MAX; count = exercise(operation);
    for (i = 0; i < count; ++i) { fail_at = i; exercise(operation); }
  }
  puts("ASDL: allocation failure cleanup passed"); return 0;
}
