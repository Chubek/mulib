/** @file mulib-tex.h
 * @brief TeX text escaping and a bounded lexical scanner, not a TeX engine.
 * Define MULIB_IMPL_TEX plus STRBUF and UTF8 switches. */
#ifndef MULIB_TEX_H
#define MULIB_TEX_H
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_TEX_TEXT = 1,
    MULIB_TEX_CONTROL = 2,
    MULIB_TEX_OPEN = 3,
    MULIB_TEX_CLOSE = 4,
    MULIB_TEX_COMMENT = 5,
    MULIB_TEX_SPACE = 6
  };
  typedef struct mulib_tex_token
  {
    int kind;
    size_t offset, length;
  } mulib_tex_token_t;
  /** @brief Cursor over borrowed UTF-8; initialize with init. No macro
   * expansion/catcodes. */
  typedef struct mulib_tex
  {
    const char *source;
    size_t length, position;
  } mulib_tex_t;
  /** @brief Validate UTF-8, reject NUL/control bytes except tab/CR/LF. 1
   * success. */
  int mulib_tex_init (mulib_tex_t *scanner, const char *source, size_t length);
  /** @brief Return 1 token, 0 EOF, -1 invalid/trailing backslash. Cursor
   * unchanged on error. ASCII letters form control words; other escaped bytes
   * form control symbols. Braces, percent comments and whitespace are separate
   * tokens. */
  int mulib_tex_next (mulib_tex_t *scanner, mulib_tex_token_t *out);
  /** @brief Append escaped literal UTF-8 for LaTeX text mode, atomically.
   * Escapes backslash, braces, # $ % & _ ^ ~; no math or raw-command
   * interpretation. */
  int mulib_tex_escape (mulib_strbuf_t *out, const char *text, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TEX) && !defined(MULIB_TEX_IMPLEMENTED)
#define MULIB_TEX_IMPLEMENTED
#include "mulib-utf8.h"
static int
mulib_tex_letter_ (unsigned char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int
mulib_tex_space_ (unsigned char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
int
mulib_tex_init (mulib_tex_t *t, const char *s, size_t n)
{
  size_t i;
  if (!t || (!s && n) || !mulib_utf8_validate ((const unsigned char *)s, n))
    return 0;
  for (i = 0; i < n; ++i)
    {
      unsigned char c = (unsigned char)s[i];
      if ((c < 32 && !mulib_tex_space_ (c)) || c == 127)
        return 0;
    }
  t->source = s;
  t->length = n;
  t->position = 0;
  return 1;
}
int
mulib_tex_next (mulib_tex_t *t, mulib_tex_token_t *out)
{
  mulib_tex_token_t v;
  size_t i;
  unsigned char c;
  if (!t || !out || (!t->source && t->length) || t->position > t->length)
    return -1;
  if (t->position == t->length)
    return 0;
  v.offset = t->position;
  i = v.offset;
  c = (unsigned char)t->source[i++];
  if (c == '\\')
    {
      if (i == t->length)
        return -1;
      v.kind = MULIB_TEX_CONTROL;
      if (mulib_tex_letter_ ((unsigned char)t->source[i]))
        while (i < t->length
               && mulib_tex_letter_ ((unsigned char)t->source[i]))
          ++i;
      else
        {
          size_t used = 1;
          uint32_t cp;
          if (!mulib_utf8_decode ((const unsigned char *)t->source + i,
                                  t->length - i, &cp, &used))
            return -1;
          i += used;
        }
    }
  else if (c == '{')
    v.kind = MULIB_TEX_OPEN;
  else if (c == '}')
    v.kind = MULIB_TEX_CLOSE;
  else if (c == '%')
    {
      v.kind = MULIB_TEX_COMMENT;
      while (i < t->length && t->source[i] != '\n' && t->source[i] != '\r')
        ++i;
    }
  else if (mulib_tex_space_ (c))
    {
      v.kind = MULIB_TEX_SPACE;
      while (i < t->length && mulib_tex_space_ ((unsigned char)t->source[i]))
        ++i;
    }
  else
    {
      v.kind = MULIB_TEX_TEXT;
      while (i < t->length && !mulib_tex_space_ ((unsigned char)t->source[i])
             && !strchr ("\\{}%", t->source[i]))
        ++i;
    }
  v.length = i - v.offset;
  t->position = i;
  *out = v;
  return 1;
}
int
mulib_tex_escape (mulib_strbuf_t *out, const char *s, size_t n)
{
  mulib_tex_t scanner;
  mulib_strbuf_t b = { 0 };
  size_t i;
  int ok = 1;
  if (!out || !mulib_tex_init (&scanner, s, n))
    return 0;
  for (i = 0; i < n && ok; ++i)
    {
      switch (s[i])
        {
        case '\\':
          ok = mulib_strbuf_append (&b, "\\textbackslash{}");
          break;
        case '^':
          ok = mulib_strbuf_append (&b, "\\textasciicircum{}");
          break;
        case '~':
          ok = mulib_strbuf_append (&b, "\\textasciitilde{}");
          break;
        case '{':
        case '}':
        case '#':
        case '$':
        case '%':
        case '&':
        case '_':
          ok = mulib_strbuf_putc (&b, '\\') && mulib_strbuf_putc (&b, s[i]);
          break;
        default:
          ok = mulib_strbuf_putc (&b, s[i]);
          break;
        }
    }
  if (ok)
    ok = mulib_strbuf_append_n (out, b.data, b.length);
  mulib_strbuf_free (&b);
  return ok;
}
#endif
