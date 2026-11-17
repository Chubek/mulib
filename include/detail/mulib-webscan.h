/* Internal allocation-free HTML subset scanner shared by web extractors.
 * Not an HTML5 tree builder, sanitizer, entity decoder, or public API. */
#ifndef MULIB_DETAIL_WEBSCAN_H
#define MULIB_DETAIL_WEBSCAN_H
#include <stddef.h>
#include <string.h>
typedef struct mulib_webscan_tag_
{
  const char *name, *attrs, *end;
  size_t length;
  int closing;
} mulib_webscan_tag_;
static inline int
mulib_webscan_space_ (unsigned char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}
static inline unsigned char
mulib_webscan_lower_ (unsigned char c)
{
  return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c;
}
static inline int
mulib_webscan_equal_ (const char *s, size_t n, const char *key)
{
  if (n != strlen (key))
    return 0;
  for (size_t i = 0; i < n; ++i)
    if (mulib_webscan_lower_ ((unsigned char)s[i])
        != mulib_webscan_lower_ ((unsigned char)key[i]))
      return 0;
  return 1;
}
/* Optional work accounting for hostile input. Bounded scanners return -1 when
 * exhausted; the original entry points retain their 1/0 behavior. */
static inline int
mulib_webscan_work_ (size_t *work)
{
  if (!work) return 1;
  if (!*work) return 0;
  --*work;
  return 1;
}
/* p points to '<'. End points at '>'; quoted '>' bytes do not terminate. */
static inline int
mulib_webscan_tag_parse_work_ (const char *p, mulib_webscan_tag_ *t, size_t *work)
{
  if (!mulib_webscan_work_ (work)) return -1;
  if (*p != '<')
    return 0;
  ++p;
  t->closing = *p == '/';
  if (t->closing)
    ++p;
  t->name = p;
  if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')))
    return 0;
  while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
         || (*p >= '0' && *p <= '9') || *p == '-' || *p == ':')
    {
      if (!mulib_webscan_work_ (work)) return -1;
      ++p;
    }
  t->length = (size_t)(p - t->name);
  if (!t->length
      || !(*p == '>' || *p == '/' || mulib_webscan_space_ ((unsigned char)*p)))
    return 0;
  t->attrs = p;
  char quote = 0;
  for (; *p; ++p)
    {
      if (!mulib_webscan_work_ (work)) return -1;
      if (quote)
        {
          if (*p == quote)
            quote = 0;
        }
      else if (*p == '\'' || *p == '"')
        quote = *p;
      else if (*p == '<')
        return 0;
      else if (*p == '>')
        {
          t->end = p;
          return 1;
        }
    }
  return 0;
}
static inline int
mulib_webscan_tag_parse_ (const char *p, mulib_webscan_tag_ *t)
{
  return mulib_webscan_tag_parse_work_ (p, t, NULL);
}
/* Exact attribute names, either quote style, arbitrary attribute order.
 * Boolean/unquoted attributes are skipped but never returned as values. */
static inline int
mulib_webscan_attr_ (const mulib_webscan_tag_ *t, const char *key,
                     const char **value, size_t *length)
{
  const char *p = t->attrs;
  while (p < t->end)
    {
      while (p < t->end && mulib_webscan_space_ ((unsigned char)*p))
        ++p;
      if (p == t->end || *p == '/')
        break;
      const char *name = p;
      while (p < t->end && !mulib_webscan_space_ ((unsigned char)*p)
             && *p != '=' && *p != '/' && *p != '\'' && *p != '"')
        ++p;
      if (p == name)
        return 0;
      size_t n = (size_t)(p - name);
      while (p < t->end && mulib_webscan_space_ ((unsigned char)*p))
        ++p;
      if (p == t->end || *p != '=')
        continue;
      ++p;
      while (p < t->end && mulib_webscan_space_ ((unsigned char)*p))
        ++p;
      if (p == t->end)
        return 0;
      char quote = *p;
      if (quote != '\'' && quote != '"')
        {
          while (p < t->end && !mulib_webscan_space_ ((unsigned char)*p))
            ++p;
          continue;
        }
      const char *a = ++p;
      while (p < t->end && *p != quote)
        ++p;
      if (p == t->end)
        return 0;
      size_t m = (size_t)(p - a);
      ++p;
      if (p < t->end && !mulib_webscan_space_ ((unsigned char)*p) && *p != '/')
        return 0;
      if (mulib_webscan_equal_ (name, n, key))
        {
          *value = a;
          *length = m;
          return 1;
        }
    }
  return 0;
}
static inline const char *
mulib_webscan_raw_end_ (const char *p, const char *name)
{
  mulib_webscan_tag_ t;
  while ((p = strchr (p, '<')) != NULL)
    {
      if (mulib_webscan_tag_parse_ (p, &t) && t.closing
          && mulib_webscan_equal_ (t.name, t.length, name))
        return p;
      ++p;
    }
  return NULL;
}
/* Opaque <!declaration> / <?instruction> subset, respecting quoted '>'. */
static inline const char *
mulib_webscan_declaration_end_ (const char *p)
{
  char quote = 0;
  for (; *p; ++p)
    {
      if (quote)
        {
          if (*p == quote)
            quote = 0;
        }
      else if (*p == '\'' || *p == '"')
        quote = *p;
      else if (*p == '>')
        return p;
    }
  return NULL;
}
/* Skip a raw/RCDATA element through its matching end tag (or EOF). The caller
 * chooses which elements are opaque; this is not an HTML5 tree builder. */
static inline int
mulib_webscan_raw_skip_work_ (const char *p, const char *name, size_t *work,
                             const char **after)
{
  while (*p)
    {
      mulib_webscan_tag_ tag;
      int parsed;
      if (!mulib_webscan_work_ (work)) return -1;
      if (*p != '<') { ++p; continue; }
      parsed = mulib_webscan_tag_parse_work_ (p, &tag, work);
      if (parsed < 0) return -1;
      if (parsed && tag.closing && mulib_webscan_equal_ (tag.name, tag.length, name))
        { *after = tag.end + 1; return 1; }
      ++p;
    }
  *after = p;
  return 1;
}
/* Skip comments and script/style contents; return complete tags in order. */
static inline int
mulib_webscan_next_work_ (const char **cursor, mulib_webscan_tag_ *t, size_t *work)
{
  const char *p = *cursor;
  while (*p)
    {
      int parsed;
      if (!mulib_webscan_work_ (work)) return -1;
      if (*p != '<') { ++p; continue; }
      if (!strncmp (p, "<!--", 4))
        {
          const char *end = p + 4;
          while (*end && strncmp (end, "-->", 3))
            { if (!mulib_webscan_work_ (work)) return -1; ++end; }
          if (!*end)
            return 0;
          p = end + 3;
          continue;
        }
      if (p[1] == '!' || p[1] == '?')
        {
          const char *end = p + 2;
          char quote = 0;
          for (; *end; ++end)
            {
              if (!mulib_webscan_work_ (work)) return -1;
              if (quote) { if (*end == quote) quote = 0; }
              else if (*end == '\'' || *end == '"') quote = *end;
              else if (*end == '>') break;
            }
          if (!*end)
            return 0;
          p = end + 1;
          continue;
        }
      parsed = mulib_webscan_tag_parse_work_ (p, t, work);
      if (parsed < 0) return -1;
      if (!parsed)
        {
          ++p;
          continue;
        }
      *cursor = t->end + 1;
      if (!t->closing
          && (mulib_webscan_equal_ (t->name, t->length, "script")
              || mulib_webscan_equal_ (t->name, t->length, "style")))
        {
          if (mulib_webscan_raw_skip_work_ (*cursor,
              mulib_webscan_equal_ (t->name, t->length, "script") ? "script" : "style",
              work, cursor) < 0) return -1;
        }
      return 1;
    }
  return 0;
}
static inline int
mulib_webscan_next_ (const char **cursor, mulib_webscan_tag_ *t)
{
  return mulib_webscan_next_work_ (cursor, t, NULL);
}
#endif
