/** @file mulib-muidl.h
 * @brief MuIDL flat-record schema source generation, not a general IDL parser.
 * Define MULIB_IMPL_MUIDL, DBSCHEMA, BUFFER and JSON dependencies.
 * Grammar: record NAME { NAME: TYPE[?][ optional]; ... }. Types are null,
 * bool, f64, string, json-array, json-object. ? means nullable. */
#ifndef MULIB_MUIDL_H
#define MULIB_MUIDL_H
#include "mulib-buffer.h"
#include "mulib-dbschema.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Atomic UTF-8 text append, no trailing NUL. 1 success, 0 invalid/OOM. */
  int mulib_muidl_generate (const mulib_dbschema_t *schema,
                            mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUIDL) && !defined(MULIB_MUIDL_IMPLEMENTED)
#define MULIB_MUIDL_IMPLEMENTED
static int
mulib_muidl_text_ (mulib_buffer_t *b, const char *s)
{
  return mulib_buffer_append (b, s, strlen (s));
}
int
mulib_muidl_generate (const mulib_dbschema_t *s, mulib_buffer_t *out)
{
  static const char *const types[]
      = { "null", "bool", "f64", "string", "json-array", "json-object" };
  mulib_buffer_t b = { 0 };
  int ok = 0;
  if (!out || !mulib_dbschema_validate (s))
    return 0;
  if (!mulib_muidl_text_ (&b, "record ") || !mulib_muidl_text_ (&b, s->name)
      || !mulib_muidl_text_ (&b, " {\n"))
    goto done;
  for (size_t i = 0; i < s->count; ++i)
    {
      const mulib_dbschema_field_t *f = s->fields + i;
      if (!mulib_muidl_text_ (&b, "  ") || !mulib_muidl_text_ (&b, f->name)
          || !mulib_muidl_text_ (&b, ": ")
          || !mulib_muidl_text_ (&b, types[f->type])
          || (f->nullable && !mulib_muidl_text_ (&b, "?"))
          || (!f->required && !mulib_muidl_text_ (&b, " optional"))
          || !mulib_muidl_text_ (&b, ";\n"))
        goto done;
    }
  ok = mulib_muidl_text_ (&b, "}\n")
       && mulib_buffer_append (out, b.data, b.length);
done:
  mulib_buffer_free (&b);
  return ok;
}
#endif
