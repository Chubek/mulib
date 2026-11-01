/* Moving realloc and exhaustive failure injection for graphics allocations. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct allocation
{
  void *pointer;
  size_t size;
} allocation;
static allocation allocations[256];
static size_t live, calls, fail_at;
static allocation *
record (void *p)
{
  for (size_t i = 0; i < 256; ++i)
    if (allocations[i].pointer == p)
      return &allocations[i];
  assert (!"unknown allocation");
  return NULL;
}
static void *
tracked_malloc (size_t n)
{
  void *p;
  allocation *r;
  if (++calls == fail_at)
    return NULL;
  p = malloc (n ? n : 1);
  if (!p)
    return NULL;
  r = record (NULL);
  r->pointer = p;
  r->size = n;
  ++live;
  return p;
}
static void
tracked_free (void *p)
{
  if (p)
    {
      allocation *r = record (p);
      r->pointer = NULL;
      --live;
      free (p);
    }
}
static void *
tracked_calloc (size_t n, size_t stride)
{
  void *p;
  if (stride && n > SIZE_MAX / stride)
    return NULL;
  p = tracked_malloc (n * stride);
  if (p)
    memset (p, 0, n * stride);
  return p;
}
static void *
tracked_realloc (void *p, size_t n)
{
  allocation *r;
  void *next;
  if (!p)
    return tracked_malloc (n);
  if (!n)
    {
      tracked_free (p);
      return NULL;
    }
  if (++calls == fail_at)
    return NULL;
  r = record (p);
  next = malloc (n);
  if (!next)
    return NULL;
  memcpy (next, p, n < r->size ? n : r->size);
  free (p);
  r->pointer = next;
  r->size = n;
  return next;
}
#define malloc tracked_malloc
#define calloc tracked_calloc
#define realloc tracked_realloc
#define free tracked_free
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_GLX
#define MULIB_IMPL_DRM
#define MULIB_IMPL_SHADERX
#define MULIB_IMPL_VECTGFX
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VEC
#include "mulib-cgeom.h"
#include "mulib-shaderx.h"
#include "mulib-vectgfx.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
static void
operation (unsigned kind)
{
  if (kind == 0)
    {
      const char s[] = "fragment { input vec2 uv; return vec4(uv,0,1); }";
      mulib_shaderx_t *p = mulib_shaderx_parse (s, sizeof (s) - 1, NULL);
      mulib_strbuf_t out;
      mulib_strbuf_init (&out);
      if (p)
        {
          (void)mulib_shaderx_emit (p, MULIB_SHADERX_GLSL330, &out);
          (void)mulib_shaderx_emit (p, MULIB_SHADERX_HLSL5, &out);
        }
      mulib_strbuf_free (&out);
      mulib_shaderx_destroy (p);
    }
  else if (kind == 1)
    {
      const char s[] = "M 1 12 C 1 0 14 0 14 12 Z";
      mulib_vectgfx_t *p = mulib_vectgfx_parse (s, sizeof (s) - 1, NULL);
      mulib_glx_t *g = mulib_glx_create (16, 16);
      if (p && g)
        {
          (void)mulib_vectgfx_fill (p, g, 0xffffffff, 0.1f);
          (void)mulib_vectgfx_stroke (p, g, 0xff000000, 2, 0.1f);
        }
      mulib_vectgfx_destroy (p);
      mulib_glx_destroy (g);
    }
  else if (kind == 2)
    {
      mulib_glx_t *g = mulib_glx_create (16, 16);
      mulib_glx_destroy (g);
    }
  else
    {
      mulib_linalg_vec2_t p[] = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, out[3];
      size_t n = 0;
      (void)mulib_cgeom_convex_hull (p, 3, out, 3, &n);
    }
}
static void
transactions (void)
{
  const char source[] = "M 1 1 L 14 1 L 14 14 Z";
  mulib_vectgfx_t *p = mulib_vectgfx_parse (source, sizeof (source) - 1, NULL);
  mulib_glx_t *g = mulib_glx_create (16, 16);
  uint32_t before[256];
  size_t base;
  assert (p && g);
  mulib_glx_clear (g, 0xff123456);
  memcpy (before, mulib_glx_pixels (g), sizeof before);
  base = live;
  for (size_t i = 1; i <= 2; ++i)
    {
      fail_at = calls + i;
      assert (!mulib_vectgfx_fill (p, g, 0xffffffff, 0.25f));
      fail_at = 0;
      assert (live == base
              && !memcmp (before, mulib_glx_pixels (g), sizeof before));
      fail_at = calls + i;
      assert (!mulib_vectgfx_stroke (p, g, 0xffffffff, 2, 0.25f));
      fail_at = 0;
      assert (live == base
              && !memcmp (before, mulib_glx_pixels (g), sizeof before));
    }
  mulib_vectgfx_destroy (p);
  mulib_glx_destroy (g);
  assert (!live);
  {
    mulib_shaderx_t *shader;
    mulib_strbuf_t out;
    const char s[] = "fragment { return vec4(1); }";
    shader = mulib_shaderx_parse (s, sizeof (s) - 1, NULL);
    assert (shader);
    mulib_strbuf_init (&out);
    assert (mulib_strbuf_append (&out, "sentinel"));
    fail_at = calls + 1;
    assert (!mulib_shaderx_emit (shader, MULIB_SHADERX_GLSL330, &out));
    fail_at = 0;
    assert (!strcmp (out.data, "sentinel"));
    mulib_strbuf_free (&out);
    mulib_shaderx_destroy (shader);
    assert (!live);
  }
}
int
main (void)
{
  for (unsigned kind = 0; kind < 4; ++kind)
    {
      size_t begin = calls, count;
      operation (kind);
      assert (!live);
      count = calls - begin;
      for (size_t failure = 1; failure <= count; ++failure)
        {
          fail_at = calls + failure;
          operation (kind);
          fail_at = 0;
          assert (!live);
        }
    }
  transactions ();
  return 0;
}
