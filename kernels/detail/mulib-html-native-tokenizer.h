/* Internal lexer for the strict, whole-input native HTML profile. */
#ifndef MULIB_DETAIL_HTML_NATIVE_TOKENIZER_H
#define MULIB_DETAIL_HTML_NATIVE_TOKENIZER_H
#include <string.h>

enum mulib_hn_token_kind_
{
  HN_EOF_, HN_TEXT_, HN_COMMENT_, HN_DOCTYPE_, HN_START_, HN_END_
};

typedef struct mulib_hn_slice_
{
  const char *data;
  size_t len;
} mulib_hn_slice_;

#define MULIB_HN_ATTR_LIMIT_ 64
typedef struct mulib_hn_token_
{
  enum mulib_hn_token_kind_ kind;
  char name[64];
  mulib_hn_slice_ text;
  struct
  {
    char name[64];
    mulib_hn_slice_ value;
  } attr[MULIB_HN_ATTR_LIMIT_];
  size_t attrs;
  int slash;
} mulib_hn_token_;

static int
mulib_hn_space_ (unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\f' || ch == '\r';
}

static unsigned char
mulib_hn_lower_ (unsigned char ch)
{
  return ch >= 'A' && ch <= 'Z' ? (unsigned char)(ch + 32) : ch;
}

static int
mulib_hn_alpha_ (unsigned char ch)
{
  ch = mulib_hn_lower_ (ch);
  return ch >= 'a' && ch <= 'z';
}

static int
mulib_hn_name_char_ (unsigned char ch)
{
  return mulib_hn_alpha_ (ch) || (ch >= '0' && ch <= '9')
         || ch == '-' || ch == '_';
}

static int
mulib_hn_ci_ (const char *s, const char *word, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    if (mulib_hn_lower_ ((unsigned char)s[i]) != (unsigned char)word[i])
      return 0;
  return 1;
}

static void
mulib_hn_skip_ (const char *s, size_t n, size_t *i)
{
  while (*i < n && mulib_hn_space_ (s[*i]))
    ++*i;
}

static mulib_html_dombridge_status_t
mulib_hn_name_ (const char *s, size_t n, size_t *i, char name[64])
{
  size_t length = 0;
  while (*i < n && mulib_hn_name_char_ (s[*i]))
    {
      if (length == 63)
        return MULIB_HTML_DOMBRIDGE_LIMIT;
      name[length++] = (char)mulib_hn_lower_ (s[(*i)++]);
    }
  name[length] = '\0';
  return length ? MULIB_HTML_DOMBRIDGE_OK : MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
}

static int
mulib_hn_markup_ (const char *s, size_t n, size_t i)
{
  return n - i >= 2 && s[i] == '<'
         && (mulib_hn_alpha_ (s[i + 1]) || s[i + 1] == '/'
             || s[i + 1] == '!' || s[i + 1] == '?');
}

static mulib_html_dombridge_status_t
mulib_hn_lex_ (const char *s, size_t n, size_t *cursor, const char *raw,
               mulib_hn_token_ *t)
{
  size_t i = *cursor, start = i;
  t->attrs = 0;
  t->slash = 0;
  t->name[0] = '\0';
  if (i == n)
    {
      t->kind = HN_EOF_;
      return MULIB_HTML_DOMBRIDGE_OK;
    }
  if (raw)
    {
      size_t k = strlen (raw);
      for (; n - i >= k + 3; ++i)
        if (s[i] == '<' && s[i + 1] == '/' && mulib_hn_ci_ (s + i + 2, raw, k)
            && (s[i + k + 2] == '>' || s[i + k + 2] == '/'
                || mulib_hn_space_ (s[i + k + 2])))
          break;
      if (n - i < k + 3)
        return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
      if (i != start)
        goto text;
    }
  if (!mulib_hn_markup_ (s, n, i))
    {
      while (++i < n && !mulib_hn_markup_ (s, n, i))
        ;
    text:
      t->kind = HN_TEXT_;
      t->text = (mulib_hn_slice_){ s + start, i - start };
      *cursor = i;
      return MULIB_HTML_DOMBRIDGE_OK;
    }
  if (n - i >= 4 && !memcmp (s + i, "<!--", 4))
    {
      start = i += 4;
      if ((i < n && s[i] == '>')
          || (n - i >= 2 && s[i] == '-' && s[i + 1] == '>'))
        return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
      for (; n - i >= 3; ++i)
        if (s[i] == '-' && s[i + 1] == '-')
          {
            if (s[i + 2] != '>')
              return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
            t->kind = HN_COMMENT_;
            t->text = (mulib_hn_slice_){ s + start, i - start };
            *cursor = i + 3;
            return MULIB_HTML_DOMBRIDGE_OK;
          }
      return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
    }
  if (s[i + 1] == '!' || s[i + 1] == '?')
    {
      if (n - i < 10 || !mulib_hn_ci_ (s + i, "<!doctype", 9)
          || !mulib_hn_space_ (s[i + 9]))
        return MULIB_HTML_DOMBRIDGE_UNSUPPORTED;
      i += 10;
      mulib_hn_skip_ (s, n, &i);
      if (n - i < 5 || !mulib_hn_ci_ (s + i, "html", 4))
        return MULIB_HTML_DOMBRIDGE_UNSUPPORTED;
      i += 4;
      mulib_hn_skip_ (s, n, &i);
      if (i == n || s[i] != '>')
        return MULIB_HTML_DOMBRIDGE_UNSUPPORTED;
      t->kind = HN_DOCTYPE_;
      *cursor = i + 1;
      return MULIB_HTML_DOMBRIDGE_OK;
    }
  ++i;
  t->kind = HN_START_;
  if (s[i] == '/')
    {
      ++i;
      t->kind = HN_END_;
    }
  if (i == n || !mulib_hn_alpha_ (s[i]))
    return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
  mulib_html_dombridge_status_t status = mulib_hn_name_ (s, n, &i, t->name);
  if (status != MULIB_HTML_DOMBRIDGE_OK)
    return status;
  for (;;)
    {
      size_t before_space = i;
      mulib_hn_skip_ (s, n, &i);
      if (i == n)
        return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
      if (s[i] == '>')
        break;
      if (t->kind == HN_END_)
        return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
      if (s[i] == '/' && n - i >= 2 && s[i + 1] == '>')
        {
          t->slash = 1;
          ++i;
          break;
        }
      if (i == before_space || (!mulib_hn_alpha_ (s[i]) && s[i] != '_'))
        return MULIB_HTML_DOMBRIDGE_UNSUPPORTED;
      if (t->attrs == MULIB_HN_ATTR_LIMIT_)
        return MULIB_HTML_DOMBRIDGE_LIMIT;
      size_t a = t->attrs++;
      status = mulib_hn_name_ (s, n, &i, t->attr[a].name);
      if (status != MULIB_HTML_DOMBRIDGE_OK)
        return status;
      size_t name_end = i;
      mulib_hn_skip_ (s, n, &i);
      if (i == n)
        return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
      t->attr[a].value = (mulib_hn_slice_){ s + i, 0 };
      if (s[i] != '=')
        {
          i = name_end; /* Separator belongs to the next iteration. */
          continue;
        }
      ++i;
      mulib_hn_skip_ (s, n, &i);
      if (i == n || s[i] == '>')
        return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
      char q = s[i] == '"' || s[i] == '\'' ? s[i++] : '\0';
      start = i;
      while (i < n && (q ? s[i] != q : s[i] != '>' && !mulib_hn_space_ (s[i])))
        {
          if (!q && (s[i] == '"' || s[i] == '\'' || s[i] == '='
                     || s[i] == '<' || s[i] == '`'))
            return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
          ++i;
        }
      if (i == n)
        return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
      t->attr[a].value = (mulib_hn_slice_){ s + start, i - start };
      if (q)
        ++i;
    }
  *cursor = i + 1;
  return MULIB_HTML_DOMBRIDGE_OK;
}
#endif
