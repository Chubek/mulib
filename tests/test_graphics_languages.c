#define MULIB_IMPL_SHADERX
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#include "mulib-shaderx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
compiler (void)
{
  const char *s
      = "// colorful fragment\nfragment { input vec2 uv; uniform float time; "
        "let v = sin(time) * 0.5 + 0.5; return vec4(uv, v, 1); }";
  const char *bad[] = { "",
                        "fragment {}",
                        "fragment { return 1; }",
                        "fragment { return vec4(missing); }",
                        "fragment { let a = vec3(1,2); return vec4(1); }",
                        "fragment { let a=1; let a=2; return vec4(a); }",
                        "fragment { return vec4(1e999); }",
                        "fragment { return vec4(1); } junk",
                        "vertex { input matrix a; return vec4(1); }",
                        "fragment { return vec4(1.2.3); }",
                        "fragment { return vec4(1/); }" };
  mulib_shaderx_error_t e;
  mulib_shaderx_t *p = mulib_shaderx_parse (s, strlen (s), &e);
  mulib_strbuf_t glsl, hlsl;
  assert (p && e.code == MULIB_SHADERX_OK);
  assert (mulib_shaderx_find_input (p, "uv") == 1
          && mulib_shaderx_find_input (p, "time") == 2
          && !mulib_shaderx_find_input (p, "absent"));
  mulib_strbuf_init (&glsl);
  mulib_strbuf_init (&hlsl);
  assert (mulib_shaderx_emit (p, MULIB_SHADERX_GLSL330, &glsl));
  assert (mulib_shaderx_emit (p, MULIB_SHADERX_HLSL5, &hlsl));
  assert (strstr (glsl.data, "#version 330 core")
          && strstr (glsl.data, "out vec4 sx_out")
          && strstr (glsl.data, "sin("));
  assert (strstr (hlsl.data, "cbuffer SXUniforms")
          && strstr (hlsl.data, "SV_Target0")
          && strstr (hlsl.data, "float4("));
  {
    size_t n = glsl.length;
    assert (!mulib_shaderx_emit (p, (mulib_shaderx_target_t)99, &glsl)
            && glsl.length == n);
  }
  mulib_shaderx_destroy (p);
  mulib_strbuf_free (&glsl);
  mulib_strbuf_free (&hlsl);
  for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
    {
      p = mulib_shaderx_parse (bad[i], strlen (bad[i]), &e);
      assert (!p && e.code != MULIB_SHADERX_OK && e.message
              && e.offset <= strlen (bad[i]));
    }
  assert (!mulib_shaderx_parse (NULL, 1, &e)
          && e.code == MULIB_SHADERX_INVALID);
  assert (!mulib_shaderx_parse ("", 65537, &e)
          && e.code == MULIB_SHADERX_LIMIT);
  {
    char nested[512];
    size_t at = 0;
    memcpy (nested, "fragment { return ", 18);
    at = 18;
    for (int i = 0; i < 100; ++i)
      nested[at++] = '(';
    memcpy (nested + at, "vec4(1)", 7);
    at += 7;
    for (int i = 0; i < 100; ++i)
      nested[at++] = ')';
    memcpy (nested + at, "; }", 3);
    at += 3;
    assert (!mulib_shaderx_parse (nested, at, &e)
            && e.code == MULIB_SHADERX_LIMIT);
  }
}
static void
graph (void)
{
  mulib_shaderx_t *p = mulib_shaderx_create (MULIB_SHADERX_VERTEX);
  mulib_shaderx_expr_t a[4], id;
  mulib_strbuf_t out;
  assert (p);
  a[0] = mulib_shaderx_input (p, MULIB_SHADERX_VEC3, "position", 0);
  a[1] = mulib_shaderx_constant (p, 1);
  assert (a[0] && a[1]);
  assert (!mulib_shaderx_input (p, MULIB_SHADERX_VEC3, "position", 0));
  assert (!mulib_shaderx_input (p, MULIB_SHADERX_FLOAT, "bad-name", 1));
  assert (!mulib_shaderx_constant (p, NAN));
  assert (!mulib_shaderx_apply (p, MULIB_SHADERX_DOT, a, 2));
  id = mulib_shaderx_vector (p, MULIB_SHADERX_VEC4, a, 2);
  assert (id && mulib_shaderx_output (p, id));
  assert (!mulib_shaderx_output (p, a[0]));
  mulib_strbuf_init (&out);
  assert (mulib_shaderx_emit (p, MULIB_SHADERX_GLSL330, &out));
  assert (strstr (out.data, "layout(location=0) in vec3")
          && strstr (out.data, "gl_Position ="));
  while (mulib_shaderx_constant (p, 1))
    {
    }
  assert (!mulib_shaderx_constant (p, 2));
  mulib_strbuf_free (&out);
  mulib_shaderx_destroy (p);
}
/* Optional external-validation mode: emit representative shaders for glslang.
 */
int
main (int argc, char **argv)
{
  if (argc == 3)
    {
      const char *source
          = "fragment { input vec2 uv; uniform float time; let "
            "n=normalize(vec3(uv,1)); let d=dot(n,n); let "
            "c=vec4(uv,sin(time),1); return mix(min(0.5,max(c,0.1)),c,d); }";
      mulib_shaderx_t *p;
      mulib_strbuf_t out;
      FILE *f;
      if (!strcmp (argv[1], "vertex"))
        source = "vertex { input vec3 position; return vec4(position,1); }";
      p = mulib_shaderx_parse (source, strlen (source), NULL);
      assert (p);
      mulib_strbuf_init (&out);
      assert (mulib_shaderx_emit (p,
                                  strstr (argv[2], "hlsl")
                                      ? MULIB_SHADERX_HLSL5
                                      : MULIB_SHADERX_GLSL330,
                                  &out));
      f = fopen (argv[2], "wb");
      assert (f);
      assert (fwrite (out.data, 1, out.length, f) == out.length);
      assert (!fclose (f));
      mulib_strbuf_free (&out);
      mulib_shaderx_destroy (p);
      return 0;
    }
  compiler ();
  graph ();
  return 0;
}
