#include "../android_runtime_setup.h"
#include <assert.h>
#include <stdio.h>
int main (void)
{
  /* iload_0; iload_1; imul; ireturn -- static int multiply(int,int). */
  const unsigned char bytes[] = {0x1a,0x1b,0x68,0xac};
  mulib_jvm_code_t code = {bytes,sizeof (bytes),2,2,2,NULL,NULL,0};
  mulib_jvm_method_t *method = NULL; const int32_t args[] = {6,7}; int32_t result;
  assert (mulib_jvm_load_code (&code,100000,&method,NULL) == MULIB_MIR_OK);
  assert (mulib_jvm_invoke (method,args,2,100000,&result) == MULIB_MIR_FINISHED && result == 42);
  printf ("JVM integer method: %d\n", (int)result);
  assert (mulib_jvm_destroy (&method) == MULIB_MIR_OK); return 0;
}
