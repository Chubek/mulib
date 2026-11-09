#include "../android_runtime_setup.h"
#include <assert.h>
#include <stdio.h>
int main (void)
{
  /* add-int v0,v1,v2; return v0. Incoming int arguments are v1,v2. */
  const unsigned char dex[] = {0x90,0,1,2,0x0f,0};
  const int32_t args[] = {20,22}; int32_t result;
  mulib_art_method_t *method = NULL;
  assert (mulib_art_load_code (dex,sizeof (dex),3,2,100000,&method,NULL) == MULIB_MIR_OK);
  assert (mulib_art_invoke (method,args,2,100000,&result) == MULIB_MIR_FINISHED && result == 42);
  printf ("ART integer method: %d; %zu ClickIR nodes\n", (int)result, mulib_art_graph (method)->graph.node_count);
  if (mulib_jit_supported ()) {
    assert (mulib_art_jit (method,NULL) == MULIB_MIR_OK);
    assert (mulib_art_invoke (method,args,2,100000,&result) == MULIB_MIR_FINISHED && result == 42);
  }
  assert (mulib_art_destroy (&method) == MULIB_MIR_OK); return 0;
}
