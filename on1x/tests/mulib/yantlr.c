/* A caller-owned, exact scalar adapter; declarations come from the real provider. */
#include "../../std/mulib/runtime.h"
#include "../../../library/yantlr/mulib-yantlr-stdlib.h"
#include <string.h>
int64_t fixture_yantlr_artifacts(const char *source) {
  mulib_yantlr_error_t error;
  mulib_yantlr_stdlib_t *library=mulib_yantlr_stdlib_create(NULL);
  mulib_yantlr_module_t *module=NULL;
  mulib_yantlr_run_t *run=NULL;
  int64_t result=-1;
  size_t count=0;
  if(!library)return -1;
  module=mulib_yantlr_parse(source,strlen(source),NULL,&error);
  if(module) {
    const mulib_yantlr_binding_t *bindings=mulib_yantlr_stdlib_bindings(library,&count);
    run=mulib_yantlr_evaluate(module,bindings,count,NULL,&error);
    if(run)result=(int64_t)mulib_yantlr_artifact_count(run);
  }
  mulib_yantlr_run_destroy(run);
  mulib_yantlr_module_destroy(module);
  mulib_yantlr_stdlib_destroy(library);
  return result;
}
