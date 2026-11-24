/** @file mulib-layoutml.h
 * @brief Strict XML layout profile: flat weighted items in a row/column.
 * Define MULIB_IMPL_LAYOUTML plus XML, UTF8, STRBUF and LAYOUT. */
#ifndef MULIB_LAYOUTML_H
#define MULIB_LAYOUTML_H
#include "mulib-layout.h"
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Parse <layout direction="row|column" gap="N"><item
   * weight="N"/>...</layout>. Unsigned decimal integers only (0..1000000), at
   * most 4096 items. Reject unknown elements/attributes/non-whitespace
   * content. Returns malloc-owned rectangles; empty layout returns NULL with
   * count=0 and success=1. Outputs unchanged on failure. error receives static
   * diagnostic. Caller frees rectangles. */
  int mulib_layoutml_parse (const char *xml, size_t length,
                            mulib_layout_rect_t parent,
                            mulib_layout_rect_t **out, size_t *count,
                            mulib_xml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LAYOUTML) && !defined(MULIB_LAYOUTML_IMPLEMENTED)
#define MULIB_LAYOUTML_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
static int
mulib_layoutml_number_ (const char *s, double *out)
{
  unsigned n = 0;
  if (!s || !*s)
    return 0;
  while (*s)
    {
      if (*s < '0' || *s > '9' || n > 100000 || (n == 100000 && *s != '0'))
        return 0;
      n = n * 10 + (unsigned)(*s++ - '0');
    }
  *out = n;
  return 1;
}
static int
mulib_layoutml_blank_ (const mulib_xml_node_t *n)
{
  const char *s = mulib_xml_value (n);
  if (mulib_xml_type (n) != MULIB_XML_TEXT || !s)
    return 0;
  while (*s)
    if (!strchr (" \t\r\n", *s++))
      return 0;
  return 1;
}
int
mulib_layoutml_parse (const char *s, size_t len, mulib_layout_rect_t p,
                      mulib_layout_rect_t **out, size_t *count,
                      mulib_xml_error_t *err)
{
  mulib_xml_node_t *doc, *root, *node;
  const char *dir, *g;
  double weights[4096], gap = 0;
  size_t n = 0, i;
  int axis, ok = 0;
  mulib_layout_rect_t *r = NULL;
  if (!out || !count)
    return 0;
  doc = mulib_xml_parse (s, len, err);
  if (!doc)
    return 0;
  root = mulib_xml_root (doc);
  if (!root || strcmp (mulib_xml_name (root), "layout"))
    goto done;
  for (node = mulib_xml_first (doc); node; node = mulib_xml_next (node))
    if (node != root && !mulib_layoutml_blank_ (node))
      goto done;
  for (i = 0; i < mulib_xml_attr_count (root); ++i)
    {
      const char *a = mulib_xml_attr_name (root, i);
      if (strcmp (a, "direction") && strcmp (a, "gap"))
        goto done;
    }
  dir = mulib_xml_attr (root, "direction");
  if (!dir)
    goto done;
  if (!strcmp (dir, "row"))
    axis = 0;
  else if (!strcmp (dir, "column"))
    axis = 1;
  else
    goto done;
  g = mulib_xml_attr (root, "gap");
  if (g && !mulib_layoutml_number_ (g, &gap))
    goto done;
  for (node = mulib_xml_first (root); node; node = mulib_xml_next (node))
    {
      if (mulib_layoutml_blank_ (node))
        continue;
      if (mulib_xml_type (node) != MULIB_XML_ELEMENT
          || strcmp (mulib_xml_name (node), "item")
          || mulib_xml_attr_count (node) != 1
          || strcmp (mulib_xml_attr_name (node, 0), "weight")
          || mulib_xml_first (node) || n == 4096
          || !mulib_layoutml_number_ (mulib_xml_attr (node, "weight"),
                                      weights + n))
        goto done;
      ++n;
    }
  if (n)
    {
      r = (mulib_layout_rect_t *)malloc (n * sizeof (*r));
      if (!r)
        goto done;
    }
  if (!mulib_layout_distribute (p, axis, gap, weights, n, r))
    goto done;
  *out = r;
  *count = n;
  r = NULL;
  ok = 1;
done:
  free (r);
  mulib_xml_destroy (doc);
  if (!ok && err)
    {
      err->offset = 0;
      err->message = "invalid layout profile, geometry, or allocation failure";
    }
  return ok;
}
#endif
