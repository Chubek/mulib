/* Exhaustive allocation-failure cleanup and transactional output checks. */
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
#include "mulib-vizigen.h"
static size_t exercise(size_t failure, int alias) {
  static const char source[] =
    "t = A1 @leave(foo) | A2 @leave(foo) | A3 @leave(foo) | A4 @leave(foo)"
    " | A5 @leave(foo) | A6 @leave(foo) | A7 @leave(foo) | A8 @leave(foo)"
    " | A9 @leave(foo) | A10 @leave(foo) | A11 @leave(foo) | A12 @leave(foo)"
    " | A13 @leave(foo) | A14 @leave(foo) | A15 @leave(foo) | A16 @leave(foo)"
    " | A17 @leave(foo)";
  mulib_buffer_t b = {NULL, 0, 0};
  mulib_asdl_error_t error;
  size_t initial, count;
  int ok;
  fail_at = SIZE_MAX;
  assert(mulib_buffer_append(&b, "sentinel", 8));
  if (alias) assert(mulib_buffer_append(&b, source, sizeof source - 1));
  initial = b.length; calls = 0; fail_at = failure;
  ok = mulib_vizigen_compile(alias ? (const char *)b.data + 8 : source,
      sizeof source - 1, NULL, &b, &error);
  count = calls; fail_at = SIZE_MAX;
  if (!ok) {
    assert(error.code == MULIB_ASDL_MEMORY && b.length == initial);
    assert(!memcmp(b.data, "sentinel", 8));
    if (alias) assert(!memcmp(b.data + 8, source, sizeof source - 1));
  } else {
    mulib_vizigen_program_t p;
    assert(mulib_vizigen_open(&p, b.data + initial, b.length - initial));
  }
  mulib_buffer_free(&b); assert(!live); return count;
}
int main(void) {
  size_t i, count;
  int alias;
  for (alias = 0; alias < 2; ++alias) {
    count = exercise(SIZE_MAX, alias);
    for (i = 0; i < count; ++i) exercise(i, alias);
  }
  return 0;
}
