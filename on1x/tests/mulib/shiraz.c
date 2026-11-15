#include "mulib-shiraz.h"
#include <stdbool.h>
#include <stdint.h>
bool fixture_shiraz_registry(void) {
  mulib_shiraz_runtime_t *runtime=mulib_shiraz_runtime_create(NULL);
  if(!runtime)return false;
  uint32_t input=42,output=0,type=0;size_t bytes=sizeof(output);
  int status=mulib_shiraz_registry_set(runtime,"HKCU\\Software\\On1x","Value",4,&input,sizeof(input));
  if(!status)status=mulib_shiraz_registry_get(runtime,"hkcu\\software\\on1x","value",&type,&output,&bytes);
  bool ok=!status&&type==4&&bytes==sizeof(output)&&output==input;
  mulib_shiraz_runtime_destroy(runtime);
  return ok;
}
