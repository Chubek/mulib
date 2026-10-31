#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-mune.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  const char *source = "x = 6; return x * 7";
  mulib_mune_t *m = mulib_mune_create ();
  double result;
  assert (m);
  assert (mulib_mune_eval (m, source, strlen (source), &result, NULL)
          == MULIB_MUNE_OK);
  assert (result == 42);
  mulib_mune_destroy (m);
  {
    const char lua[] =
        "local function sum(...)\n"
        "  local result = 0\n"
        "  for _, value in ipairs({...}) do result = result + value end\n"
        "  return result\n"
        "end\n"
        "return sum(10, 20, 12)\n";
    mulib_mune_error_t error;
    mulib_mune_chunk_t *chunk = mulib_mune_parse (lua, sizeof (lua) - 1, NULL, &error);
    mulib_mune_state_t *state = mulib_mune_state_create (NULL);
    mulib_mune_result_t *values = NULL;
    int mode;
    assert (chunk && error.code == MULIB_MUNE_OK);
    assert (state);
    assert (mulib_mune_chunk_ast (chunk)->length == sizeof (lua) - 1);
    for (mode = MULIB_MUNE_TREE; mode <= MULIB_MUNE_JIT; ++mode)
      {
        if (mode == MULIB_MUNE_JIT && !mulib_jit_supported ()) continue;
        assert (mulib_mune_set_mode (state, (mulib_mune_mode_t)mode) == MULIB_MUNE_OK);
        assert (mulib_mune_run (state, chunk, &values, &error) == MULIB_MUNE_OK);
        assert (mulib_mune_result_count (values) == 1);
        assert (mulib_mune_result_get (values, 0).type == MULIB_MUNE_INTEGER);
        assert (mulib_mune_result_get (values, 0).as.integer == 42);
        mulib_mune_result_destroy (values); values = NULL;
      }
    mulib_mune_state_destroy (state);
    mulib_mune_chunk_destroy (chunk);
  }
  return 0;
}
