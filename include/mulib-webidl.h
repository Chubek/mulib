/** @file mulib-webidl.h
 * @brief Flat schema to WebIDL dictionary source generation.
 * Define MULIB_IMPL_WEBIDL, DBSCHEMA, BUFFER and JSON dependencies.
 * No WebIDL parser, interfaces or bindings. Null-only fields unsupported;
 * arrays become sequence<any>; nested objects become object. Identifiers are
 * escaped with WebIDL's leading underscore to avoid keyword collisions. */
#ifndef MULIB_WEBIDL_H
#define MULIB_WEBIDL_H
#include "mulib-buffer.h"
#include "mulib-dbschema.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Atomic text append, no trailing NUL. */
  int mulib_webidl_generate (const mulib_dbschema_t *schema,
                             mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBIDL) && !defined(MULIB_WEBIDL_IMPLEMENTED)
#define MULIB_WEBIDL_IMPLEMENTED
static int
mulib_webidl_text_ (mulib_buffer_t *b, const char *s)
{
  return mulib_buffer_append (b, s, strlen (s));
}
int
mulib_webidl_generate (const mulib_dbschema_t *s, mulib_buffer_t *out)
{
  static const char *const types[]
      = { NULL, "boolean", "double", "DOMString", "sequence<any>", "object" };
  mulib_buffer_t b = { 0 };
  int ok = 0;
  if (!out || !mulib_dbschema_validate (s))
    return 0;
  if (!mulib_webidl_text_ (&b, "dictionary _")
      || !mulib_webidl_text_ (&b, s->name) || !mulib_webidl_text_ (&b, " {\n"))
    goto done;
  for (size_t i = 0; i < s->count; ++i)
    {
      const mulib_dbschema_field_t *f = s->fields + i;
      if (!types[f->type] || !mulib_webidl_text_ (&b, "  ")
          || (f->required && !mulib_webidl_text_ (&b, "required "))
          || !mulib_webidl_text_ (&b, types[f->type])
          || (f->nullable && !mulib_webidl_text_ (&b, "?"))
          || !mulib_webidl_text_ (&b, " _") || !mulib_webidl_text_ (&b, f->name)
          || !mulib_webidl_text_ (&b, ";\n"))
        goto done;
    }
  ok = mulib_webidl_text_ (&b, "};\n")
       && mulib_buffer_append (out, b.data, b.length);
done:
  mulib_buffer_free (&b);
  return ok;
}
#endif
