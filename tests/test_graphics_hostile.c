/* Deterministic mutation corpus: bounded parser work, diagnostics and cleanup. */
#define MULIB_IMPL_SHADERX
#define MULIB_IMPL_VECTGFX
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VEC
#define MULIB_IMPL_GLX
#define MULIB_IMPL_DRM
#define MULIB_IMPL_LINALG
#include "mulib-shaderx.h"
#include "mulib-vectgfx.h"
#include <assert.h>
#include <string.h>

int main (void)
{
  const char *seeds[] = {
    "fragment { input vec2 uv; return vec4(uv,0,1); }",
    "vertex { let x=normalize(vec3(1)); return vec4(x,1); }",
    "fragment { return vec4(dot()); }",
    "M 1 1 C 2 3 8 9 10 10 Z",
    "M 0 0 S 1 2 3 4 5 6 A 8 8 4 4 0 6 Z"
  };
  uint32_t random = 1234567;
  mulib_glx_t *surface = mulib_glx_create (16,16);
  assert (surface);
  for (size_t iteration=0; iteration<2000; ++iteration)
    {
      char source[128]; size_t length=strlen(seeds[iteration%5]);
      mulib_shaderx_error_t se; mulib_vectgfx_error_t ve;
      mulib_shaderx_t *shader; mulib_vectgfx_t *path;
      memcpy(source,seeds[iteration%5],length);
      for (unsigned mutation=0; mutation<1+iteration%4; ++mutation)
        {
          random=random*1664525u+1013904223u;
          source[random%length]=(char)(random>>24);
        }
      if (iteration%3==0) length=iteration%length;
      shader=mulib_shaderx_parse(source,length,&se);
      if (shader)
        {
          mulib_strbuf_t output; mulib_strbuf_init(&output);
          assert(mulib_shaderx_emit(shader,MULIB_SHADERX_GLSL330,&output));
          assert(mulib_shaderx_emit(shader,MULIB_SHADERX_HLSL5,&output));
          mulib_strbuf_free(&output);
        }
      else assert(se.code!=MULIB_SHADERX_OK&&se.message&&se.offset<=length);
      mulib_shaderx_destroy(shader);
      path=mulib_vectgfx_parse(source,length,&ve);
      if(path)
        {
          (void)mulib_vectgfx_fill(path,surface,0xffffffff,0.25f);
          (void)mulib_vectgfx_stroke(path,surface,0xffffffff,2,0.25f);
        }
      else assert(ve.message&&ve.offset<=length);
      mulib_vectgfx_destroy(path);
    }
  mulib_glx_destroy(surface);
  return 0;
}
