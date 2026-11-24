/** @file mulib-dbschema.h
 * @brief Borrowed flat record schemas over the shared JSON value model.
 * Define MULIB_IMPL_DBSCHEMA and JSON dependencies. Exact JSON types, no
 * coercion; nullable permits explicit null, required controls absent fields. */
#ifndef MULIB_DBSCHEMA_H
#define MULIB_DBSCHEMA_H
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dbschema_field
  {
    const char *name;
    mulib_json_type type;
    int required, nullable;
  } mulib_dbschema_field_t;
  typedef struct mulib_dbschema
  {
    const char *name;
    const mulib_dbschema_field_t *fields;
    size_t count;
  } mulib_dbschema_t;
  int mulib_dbschema_identifier (const char *name);
  int mulib_dbschema_validate (const mulib_dbschema_t *schema);
  /** Reject unknown fields, missing required fields and wrong types. Nested
   * object/array contents are not constrained. */
  int mulib_dbschema_check (const mulib_dbschema_t *schema,
                            const mulib_json_t *record);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBSCHEMA) && !defined(MULIB_DBSCHEMA_IMPLEMENTED)
#define MULIB_DBSCHEMA_IMPLEMENTED
#include <string.h>
int
mulib_dbschema_identifier (const char *s)
{
  unsigned c;
  if (!s || !*s)
    return 0;
  c = (unsigned char)*s;
  if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'))
    return 0;
  for (++s; *s; ++s)
    {
      c = (unsigned char)*s;
      if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '_'))
        return 0;
    }
  return 1;
}
int
mulib_dbschema_validate (const mulib_dbschema_t *s)
{
  if (!s || !mulib_dbschema_identifier (s->name) || (!s->fields && s->count))
    return 0;
  for (size_t i = 0; i < s->count; ++i)
    {
      const mulib_dbschema_field_t *f = s->fields + i;
      if (!mulib_dbschema_identifier (f->name) || f->type < MULIB_JSON_NULL
          || f->type > MULIB_JSON_OBJECT
          || (f->required != 0 && f->required != 1)
          || (f->nullable != 0 && f->nullable != 1))
        return 0;
      for (size_t j = 0; j < i; ++j)
        if (!strcmp (s->fields[j].name, f->name))
          return 0;
    }
  return 1;
}
int
mulib_dbschema_check (const mulib_dbschema_t *s, const mulib_json_t *v)
{
  if (!mulib_dbschema_validate (s)
      || mulib_json_typeof (v) != MULIB_JSON_OBJECT)
    return 0;
  for (size_t i = 0; i < s->count; ++i)
    {
      const mulib_dbschema_field_t *f = s->fields + i;
      const mulib_json_t *x = mulib_json_get (v, f->name);
      if (!x)
        {
          if (f->required)
            return 0;
        }
      else if (mulib_json_typeof (x) != f->type
               && !(f->nullable && mulib_json_typeof (x) == MULIB_JSON_NULL))
        return 0;
    }
  for (size_t i = 0; i < mulib_json_size (v); ++i)
    {
      size_t n, j;
      const char *key = mulib_json_key (v, i, &n);
      for (j = 0; j < s->count; ++j)
        if (strlen (s->fields[j].name) == n
            && !memcmp (key, s->fields[j].name, n))
          break;
      if (j == s->count)
        return 0;
    }
  return 1;
}
#endif
