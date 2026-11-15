/* Offline execution through the canonical engine and its embedded lmuw3. */
#include "mulib-muw3-mune.h"
#include <stdbool.h>
#include <string.h>
bool fixture_muw3_script(const char *source) {
  mulib_mune_error_t error;
  mulib_mune_result_t *result=NULL;
  mulib_muw3_engine_t *engine=mulib_muw3_engine_create(NULL,NULL,&error);
  bool ok=false;
  if(!engine)return false;
  if(mulib_muw3_engine_run(engine,source,strlen(source),&result,&error)==MULIB_MUNE_OK) {
    mulib_mune_value_t value=mulib_mune_result_get(result,0);
    ok=value.type==MULIB_MUNE_BOOLEAN&&value.as.boolean;
  }
  mulib_mune_result_destroy(result);
  mulib_muw3_engine_destroy(engine);
  return ok;
}
