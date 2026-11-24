/** @file mulib-wit.h
 * @brief Flat primitive schemas to WIT record source, not component bindings.
 * Define MULIB_IMPL_WIT, DBSCHEMA, BUFFER and JSON dependencies.
 * bool/f64/string only. Lowercase identifier words separated by underscores
 * become kebab-case; all names use % escaping. Missing and nullable each add
 * one option layer, preserving both distinctions when both are permitted. */
#ifndef MULIB_WIT_H
#define MULIB_WIT_H
#include "mulib-buffer.h"
#include "mulib-dbschema.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_wit_generate (const mulib_dbschema_t *schema, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WIT) && !defined(MULIB_WIT_IMPLEMENTED)
#define MULIB_WIT_IMPLEMENTED
static int
mulib_wit_text_ (mulib_buffer_t *b, const char *s)
{
  return mulib_buffer_append (b, s, strlen (s));
}
static int
mulib_wit_name_ (mulib_buffer_t *b, const char *s)
{
  int first = 1;
  if (!mulib_wit_text_ (b, "%"))
    return 0;
  for (; *s; ++s)
    {
      char c = *s;
      if (c == '_')
        {
          if (first)
            return 0;
          c = '-';
          first = 1;
        }
      else
        {
          if (!((c >= 'a' && c <= 'z') || (!first && c >= '0' && c <= '9')))
            return 0;
          first = 0;
        }
      if (!mulib_buffer_append (b, &c, 1))
        return 0;
    }
  return !first;
}
int
mulib_wit_generate (const mulib_dbschema_t *s, mulib_buffer_t *out)
{
  static const char *const types[]
      = { NULL, "bool", "f64", "string", NULL, NULL };
  mulib_buffer_t b = { 0 };
  int ok = 0;
  if (!out || !mulib_dbschema_validate (s))
    return 0;
  if (!mulib_wit_text_ (&b, "record ") || !mulib_wit_name_ (&b, s->name)
      || !mulib_wit_text_ (&b, " {\n"))
    goto done;
  for (size_t i = 0; i < s->count; ++i)
    {
      const mulib_dbschema_field_t *f = s->fields + i;
      if (!types[f->type] || !mulib_wit_text_ (&b, "  ")
          || !mulib_wit_name_ (&b, f->name) || !mulib_wit_text_ (&b, ": ")
          || (!f->required && !mulib_wit_text_ (&b, "option<"))
          || (f->nullable && !mulib_wit_text_ (&b, "option<"))
          || !mulib_wit_text_ (&b, types[f->type])
          || (f->nullable && !mulib_wit_text_ (&b, ">"))
          || (!f->required && !mulib_wit_text_ (&b, ">"))
          || !mulib_wit_text_ (&b, ",\n"))
        goto done;
    }
  ok = mulib_wit_text_ (&b, "}\n")
       && mulib_buffer_append (out, b.data, b.length);
done:
  mulib_buffer_free (&b);
  return ok;
}
#endif
