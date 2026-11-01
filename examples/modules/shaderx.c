#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_SHADERX
#include "mulib-shaderx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int
main (void)
{
  const char *source = "fragment { input vec2 uv; uniform float time; return "
                       "vec4(uv, sin(time)*0.5+0.5, 1); }";
  mulib_shaderx_error_t error;
  mulib_strbuf_t output;
  mulib_shaderx_t *shader
      = mulib_shaderx_parse (source, strlen (source), &error);
  assert (shader && error.code == MULIB_SHADERX_OK);
  mulib_strbuf_init (&output);
  assert (mulib_shaderx_emit (shader, MULIB_SHADERX_GLSL330, &output));
  puts (output.data);
  mulib_strbuf_free (&output);
  mulib_shaderx_destroy (shader);
  return 0;
}
