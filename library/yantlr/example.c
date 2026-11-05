/* cc -std=c11 -Iinclude library/yantlr/example.c -lm -o yantlr-demo */
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_YANTLR
#define MULIB_IMPL_YANTLR_STDLIB
#include "mulib-yantlr-stdlib.h"

int main(int argc, char **argv) {
  mulib_yantlr_stdlib_t *library = mulib_yantlr_stdlib_create(NULL);
  mulib_yantlr_module_t *module;
  mulib_yantlr_run_t *run;
  mulib_yantlr_error_t error;
  const mulib_yantlr_binding_t *bindings;
  size_t count, i;
  int success;
  if (!library) { fputs("cannot allocate standard library\n", stderr); return 1; }
  module = mulib_yantlr_load(argc > 1 ? argv[1] : "library/yantlr/example.y1", NULL, &error);
  if (!module) {
    fprintf(stderr, "%zu:%zu: %s\n", error.line, error.column, error.message);
    mulib_yantlr_stdlib_destroy(library); return 1;
  }
  bindings = mulib_yantlr_stdlib_bindings(library, &count);
  run = mulib_yantlr_evaluate(module, bindings, count, NULL, &error);
  if (!run) fprintf(stderr, "%zu:%zu: %s\n", error.line, error.column, error.message);
  for (i = 0; i < mulib_yantlr_artifact_count(run); ++i) {
    const mulib_yantlr_artifact_t *a = mulib_yantlr_artifact(run, i);
    printf("%s (%zu bytes)\n", a->path, a->length);
    if (fwrite(a->data, 1, a->length, stdout) != a->length) break;
    putchar('\n');
  }
  count = mulib_yantlr_artifact_count(run);
  success = run != NULL && i == count && !ferror(stdout);
  mulib_yantlr_run_destroy(run); mulib_yantlr_module_destroy(module);
  mulib_yantlr_stdlib_destroy(library);
  return success ? 0 : 1;
}
