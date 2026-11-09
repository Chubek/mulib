/** @file mulib-mathml.h
 * @brief Presentation MathML DOM construction, structural validation and XML
 * I/O. Define MULIB_IMPL_MATHML, XML, STRBUF and UTF8. No layout or
 * evaluation.
 */
#ifndef MULIB_MATHML_H
#define MULIB_MATHML_H
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_MATHML_NAMESPACE "http://www.w3.org/1998/Math/MathML"
  /** @brief Diagnostic: borrowed node and static message. Parse errors also
   * have a byte offset. On parse failure node is NULL (the temporary DOM was
   * freed). */
  typedef struct mulib_mathml_error
  {
    const mulib_xml_node_t *node;
    size_t offset;
    const char *message;
  } mulib_mathml_error_t;
  /** @brief Allocate math root with namespace and inline/block display. */
  mulib_xml_node_t *mulib_mathml_new (int block);
  /** @brief Allocate a supported non-token element, initially empty. Use
   * mulib_xml_append to transfer child ownership and complete its arity. */
  mulib_xml_node_t *mulib_mathml_element (const char *name);
  /** @brief Allocate mi/mn/mo/mtext/ms with copied UTF-8 text, NULL on
   * failure. */
  mulib_xml_node_t *mulib_mathml_token (const char *name, const char *text);
  /** @brief Validate a math root or XML document against the documented
   * subset. Returns 1/0; checks namespace, elements, attributes, arity and
   * table structure. */
  int mulib_mathml_validate (const mulib_xml_node_t *root,
                             mulib_mathml_error_t *error);
  /** @brief Parse and validate length bytes into an owned XML document.
   * Destroy with mulib_xml_destroy. NULL on error, with optional diagnostic.
   */
  mulib_xml_node_t *mulib_mathml_parse (const char *source, size_t length,
                                        mulib_mathml_error_t *error);
  /** @brief Validate and serialize to malloc-owned UTF-8; free with free().
   * Length excludes NUL and becomes zero on failure. */
  char *mulib_mathml_serialize (const mulib_xml_node_t *root, size_t *length,
                                mulib_mathml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MATHML) && !defined(MULIB_MATHML_IMPLEMENTED)
#define MULIB_MATHML_IMPLEMENTED
#include <string.h>
/* token=-1; variable child count=0; other values are exact arities. */
static int
mulib_mathml_kind_ (const char *name)
{
  static const char *const names[]
      = { "math",   "mrow",   "msqrt",   "mstyle",    "mphantom", "mtable",
          "mtr",    "mtd",    "mi",      "mn",        "mo",       "mtext",
          "ms",     "mspace", "mfrac",   "mroot",     "msub",     "msup",
          "munder", "mover",  "msubsup", "munderover" };
  size_t i;
  if (!name)
    return -100;
  for (i = 0; i < sizeof (names) / sizeof (names[0]); ++i)
    if (!strcmp (name, names[i]))
      {
        if (i < 8)
          return 0;
        if (i < 13)
          return -1;
        if (i == 13)
          return -2;
        return i < 20 ? 2 : 3;
      }
  return -100;
}
static int
mulib_mathml_fail_ (mulib_mathml_error_t *e, const mulib_xml_node_t *n,
                    const char *m)
{
  if (e)
    {
      e->node = n;
      e->offset = 0;
      e->message = m;
    }
  return 0;
}
static int
mulib_mathml_space_ (const char *s)
{
  while (*s)
    {
      if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
        return 0;
      ++s;
    }
  return 1;
}
static int
mulib_mathml_attr_ (const char *tag, const char *name, const char *value,
                    int root)
{
  if (!strcmp (name, "xmlns"))
    return root && !strcmp (value, MULIB_MATHML_NAMESPACE);
  if (!strcmp (name, "display"))
    return root && (!strcmp (value, "inline") || !strcmp (value, "block"));
  if (!strcmp (name, "id") || !strcmp (name, "class")
      || !strcmp (name, "mathcolor") || !strcmp (name, "mathbackground"))
    return 1;
  if (!strcmp (name, "mathvariant"))
    {
      static const char *const variants[] = { "normal",
                                              "bold",
                                              "italic",
                                              "bold-italic",
                                              "double-struck",
                                              "bold-fraktur",
                                              "script",
                                              "bold-script",
                                              "fraktur",
                                              "sans-serif",
                                              "bold-sans-serif",
                                              "sans-serif-italic",
                                              "sans-serif-bold-italic",
                                              "monospace",
                                              "initial",
                                              "tailed",
                                              "looped",
                                              "stretched" };
      size_t i;
      for (i = 0; i < sizeof (variants) / sizeof (variants[0]); ++i)
        if (!strcmp (value, variants[i]))
          return 1;
      return 0;
    }
  if (!strcmp (name, "displaystyle"))
    return !strcmp (value, "true") || !strcmp (value, "false");
  if (!strcmp (tag, "mo"))
    {
      if (!strcmp (name, "form"))
        return !strcmp (value, "prefix") || !strcmp (value, "infix")
               || !strcmp (value, "postfix");
      if (!strcmp (name, "stretchy") || !strcmp (name, "fence")
          || !strcmp (name, "separator") || !strcmp (name, "symmetric")
          || !strcmp (name, "largeop") || !strcmp (name, "movablelimits"))
        return !strcmp (value, "true") || !strcmp (value, "false");
      if (!strcmp (name, "lspace") || !strcmp (name, "rspace"))
        return 1;
    }
  if (!strcmp (tag, "mspace")
      && (!strcmp (name, "width") || !strcmp (name, "height")
          || !strcmp (name, "depth")))
    return 1;
  if (!strcmp (tag, "mfrac") && !strcmp (name, "linethickness"))
    return 1;
  return 0;
}
static int
mulib_mathml_walk_ (const mulib_xml_node_t *n, size_t depth, int root,
                    mulib_mathml_error_t *e)
{
  const char *name = mulib_xml_name (n);
  const mulib_xml_node_t *child;
  size_t i, count = 0;
  int kind = mulib_mathml_kind_ (name);
  if (depth > MULIB_XML_DEPTH_LIMIT)
    return mulib_mathml_fail_ (e, n, "MathML depth limit");
  if (kind == -100)
    return mulib_mathml_fail_ (e, n, "unsupported MathML element");
  if (!root && !strcmp (name, "math"))
    return mulib_mathml_fail_ (e, n, "nested math root");
  for (i = 0; i < mulib_xml_attr_count (n); ++i)
    if (!mulib_mathml_attr_ (name, mulib_xml_attr_name (n, i),
                             mulib_xml_attr_value (n, i), root))
      return mulib_mathml_fail_ (e, n,
                                 "unsupported MathML attribute or value");
  for (child = mulib_xml_first (n); child; child = mulib_xml_next (child))
    {
      mulib_xml_type_t type = mulib_xml_type (child);
      if (type == MULIB_XML_COMMENT)
        continue;
      if (type == MULIB_XML_TEXT || type == MULIB_XML_CDATA)
        {
          if (kind != -1 && !mulib_mathml_space_ (mulib_xml_value (child)))
            return mulib_mathml_fail_ (e, child, "text outside MathML token");
          continue;
        }
      if (kind < 0)
        return mulib_mathml_fail_ (e, child,
                                   "element inside MathML token or space");
      if (type != MULIB_XML_ELEMENT)
        return mulib_mathml_fail_ (e, child, "invalid MathML child");
      if ((!strcmp (name, "mtable") && strcmp (mulib_xml_name (child), "mtr"))
          || (!strcmp (name, "mtr") && strcmp (mulib_xml_name (child), "mtd"))
          || (!strcmp (mulib_xml_name (child), "mtr")
              && strcmp (name, "mtable"))
          || (!strcmp (mulib_xml_name (child), "mtd") && strcmp (name, "mtr")))
        return mulib_mathml_fail_ (e, child, "invalid MathML table structure");
      if (!mulib_mathml_walk_ (child, depth + 1, 0, e))
        return 0;
      ++count;
    }
  if ((kind > 0 && count != (size_t)kind)
      || ((!strcmp (name, "mtr") || !strcmp (name, "mtable")) && !count))
    return mulib_mathml_fail_ (e, n, "incorrect MathML child count");
  return 1;
}
mulib_xml_node_t *
mulib_mathml_new (int block)
{
  mulib_xml_node_t *n;
  if (block != 0 && block != 1)
    return NULL;
  n = mulib_xml_new (MULIB_XML_ELEMENT, "math");
  if (n
      && (!mulib_xml_set_attr (n, "xmlns", MULIB_MATHML_NAMESPACE)
          || !mulib_xml_set_attr (n, "display", block ? "block" : "inline")))
    {
      mulib_xml_destroy (n);
      return NULL;
    }
  return n;
}
mulib_xml_node_t *
mulib_mathml_element (const char *name)
{
  int kind = mulib_mathml_kind_ (name);
  if (kind == -100 || kind == -1 || !strcmp (name, "math"))
    return NULL;
  return mulib_xml_new (MULIB_XML_ELEMENT, name);
}
mulib_xml_node_t *
mulib_mathml_token (const char *name, const char *text)
{
  mulib_xml_node_t *n, *t;
  if (mulib_mathml_kind_ (name) != -1 || !text)
    return NULL;
  n = mulib_xml_new (MULIB_XML_ELEMENT, name);
  if (!n)
    return NULL;
  t = mulib_xml_new (MULIB_XML_TEXT, text);
  if (!t || !mulib_xml_append (n, t))
    {
      mulib_xml_destroy (t);
      mulib_xml_destroy (n);
      return NULL;
    }
  return n;
}
int
mulib_mathml_validate (const mulib_xml_node_t *root, mulib_mathml_error_t *e)
{
  const char *ns;
  if (e)
    memset (e, 0, sizeof (*e));
  if (root && mulib_xml_type (root) == MULIB_XML_DOCUMENT)
    root = mulib_xml_root (root);
  if (!root || !mulib_xml_name (root)
      || strcmp (mulib_xml_name (root), "math"))
    return mulib_mathml_fail_ (e, root, "expected math root");
  ns = mulib_xml_attr (root, "xmlns");
  if (!ns || strcmp (ns, MULIB_MATHML_NAMESPACE))
    return mulib_mathml_fail_ (e, root, "expected MathML default namespace");
  return mulib_mathml_walk_ (root, 1, 1, e);
}
mulib_xml_node_t *
mulib_mathml_parse (const char *source, size_t length, mulib_mathml_error_t *e)
{
  mulib_xml_error_t xe;
  mulib_xml_node_t *doc;
  if (e)
    memset (e, 0, sizeof (*e));
  doc = mulib_xml_parse (source, length, &xe);
  if (!doc)
    {
      if (e)
        {
          e->offset = xe.offset;
          e->message = xe.message;
        }
      return NULL;
    }
  if (!mulib_mathml_validate (doc, e))
    {
      if (e)
        e->node = NULL;
      mulib_xml_destroy (doc);
      return NULL;
    }
  return doc;
}
char *
mulib_mathml_serialize (const mulib_xml_node_t *root, size_t *length,
                        mulib_mathml_error_t *e)
{
  char *s;
  if (length)
    *length = 0;
  if (!mulib_mathml_validate (root, e))
    return NULL;
  s = mulib_xml_serialize (root, length);
  if (!s)
    (void)mulib_mathml_fail_ (e, root, "MathML serialization failed");
  return s;
}
#endif
