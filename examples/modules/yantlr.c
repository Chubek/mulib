#define MULIB_IMPL_ARENA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_YANTLR
#include "mulib-yantlr.h"
#include <assert.h>

/* An application-supplied provider, NOT a YANTLR standard-library service.
 * Real providers can use MuLIB's grammar, IR, rewrite and codegen modules. */
static int emit_c(mulib_yantlr_run_t *run,
    const mulib_yantlr_value_t *const *args, size_t count,
    const mulib_yantlr_value_t **out, void *user) {
  const char *source;
  size_t length;
  (void)user; assert(count == 1);
  source = mulib_yantlr_text(mulib_yantlr_member(args[0], "source"), &length);
  if (!source) return mulib_yantlr_fail(run, MULIB_YANTLR_TYPE, "EmitC requires a source string");
  if (!mulib_yantlr_emit(run, "demo.c", source, length)) return 0;
  *out = args[0]; return 1;
}
int main(void) {
  const char *spec = "intermediary \"Demo\";\n"
      "set Build Host[Example]::EmitC!{\"source\":\"int main(void) { return 0; }\\n\"};";
  mulib_yantlr_binding_t provider = {"Host[Example]::EmitC", 1, emit_c, NULL, NULL};
  mulib_yantlr_error_t error;
  mulib_yantlr_module_t *module = mulib_yantlr_parse(spec, strlen(spec), NULL, &error);
  mulib_yantlr_run_t *run;
  const mulib_yantlr_artifact_t *artifact;
  assert(module && mulib_yantlr_dependency_count(module) == 1);
  run = mulib_yantlr_evaluate(module, &provider, 1, NULL, &error); assert(run);
  artifact = mulib_yantlr_artifact(run, 0); assert(artifact);
  printf("%s -> %s (%zu bytes)\n", mulib_yantlr_name(module), artifact->path, artifact->length);
  assert(fwrite(artifact->data, 1, artifact->length, stdout) == artifact->length);
  mulib_yantlr_run_destroy(run); mulib_yantlr_module_destroy(module); return 0;
}
