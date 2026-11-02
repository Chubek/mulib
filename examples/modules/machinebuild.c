#define MULIB_IMPL_VEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_FST
#define MULIB_IMPL_MACHINEBUILD
#include "mulib-machinebuild.h"
#include <assert.h>

int main (void)
{
  const char source[] =
    "machine switch {\n"
    "  state off accept; state on accept; start off;\n"
    "  transition off -> on on 1 emit 1;\n"
    "  transition on -> off on 0 emit 0;\n"
    "}\n";
  mulib_machinebuild_t *machine = NULL;
  mulib_machinebuild_diagnostic_t diagnostic;
  mulib_strbuf_t js;
  mulib_strbuf_init (&js);
  assert (mulib_machinebuild_parse (source, sizeof (source) - 1, NULL,
                                   &machine, &diagnostic) == MULIB_MACHINEBUILD_OK);
  assert (mulib_machinebuild_emit_js (machine, &js) == MULIB_MACHINEBUILD_OK);
  /* The emitted expression returns a factory: const createSwitch = <expression>; */
  assert (fwrite (js.data, 1, js.length, stdout) == js.length);
  mulib_strbuf_free (&js);
  mulib_machinebuild_destroy (machine);
  return 0;
}
