/* These two explicit implementation TUs can instead be linked as static libraries. */
#include "../../library/microchan/runtime.c"
#include "../../library/lmuw3/runtime.c"
#include <assert.h>
int main (void)
{
  static const char source[] =
    "local w = require('lmuw3')\n"
    "local config = w.yaml_decode('name: Weblet\\nrequests: []\\n')\n"
    "return w.json_encode(config)\n";
  mulib_muw3_engine_t *engine = mulib_muw3_engine_create (NULL, NULL, NULL);
  mulib_mune_result_t *result = NULL;
  const char *text;
  assert (engine);
  assert (!mulib_muw3_engine_run (engine, source, sizeof source - 1, &result, NULL));
  text = mulib_mune_value_string (mulib_mune_result_get (result, 0), NULL);
  assert (text); puts (text);
  mulib_mune_result_destroy (result); mulib_muw3_engine_destroy (engine);
  return 0;
}
