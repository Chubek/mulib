#define MULIB_IMPL_AST
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_TERMSCRIPT_AST
#define MULIB_IMPL_TERMSCRIPT_LIBRARY
#include "mulib-termscript-library.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>

static void
append(mulib_strbuf_t *path, const char *root, size_t root_n, const char *suffix)
{
  assert(mulib_strbuf_append_n(path, root, root_n) && mulib_strbuf_append(path, suffix));
}

static int64_t
integer(const mulib_termscript_program_t *program)
{
  for (size_t i = program->root; i != SIZE_MAX; i = program->nodes[i].next)
    if (program->nodes[i].kind == MULIB_TERMSCRIPT_INTEGER) return program->nodes[i].integer;
  assert(0 && "fixture contains no integer");
  return 0;
}

int
main(void)
{
  const char *source = __FILE__, *test_dir = strstr(source, "tests/test_termscript_library.c");
  mulib_strbuf_t first = { NULL, 0, 0 }, second = { NULL, 0, 0 }, malformed = { NULL, 0, 0 };
  mulib_termscript_program_t p = { 0 };
  mulib_termscript_error_t e;
  assert(test_dir);
  size_t root_n = (size_t)(test_dir - source);
  append(&second, source, root_n, "tests/fixtures/termscript/second");
  /* Empty path components must not change search precedence. */
  assert(mulib_strbuf_putc(&first, ':'));
  append(&first, source, root_n, "tests/fixtures/termscript/first::");
  assert(mulib_strbuf_append(&first, second.data));
  append(&malformed, source, root_n, "tests/fixtures/termscript/broken:");
  assert(mulib_strbuf_append(&malformed, second.data));
  assert(mulib_termscript_library_load(first.data, NULL, "testlib.module", 14, &p, &e));
  assert(integer(&p) == 11);
  assert(!mulib_termscript_library_load(first.data, NULL, "testlib.other", 13, &p, &e));
  assert(integer(&p) == 11 && e.message);
  mulib_termscript_program_free(&p);
  assert(mulib_termscript_library_load(first.data, NULL, "testlib.other", 13, &p, &e));
  assert(integer(&p) == 33);
  mulib_termscript_program_free(&p);
  assert(!mulib_termscript_library_load(malformed.data, NULL, "testlib.module", 14, &p, &e));
  assert(!p.source && e.message);
  assert(!mulib_termscript_library_load(first.data, NULL, "../module", 9, &p, &e));
  assert(!mulib_termscript_library_load(first.data, NULL, "testlib\0module", 14, &p, &e));
  assert(mulib_termscript_library_load("", second.data, "testlib.module", 14, &p, &e));
  assert(integer(&p) == 22);
  mulib_termscript_program_free(&p);
  mulib_strbuf_free(&malformed);
  mulib_strbuf_free(&second);
  mulib_strbuf_free(&first);
  return 0;
}
