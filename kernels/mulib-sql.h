/** @file mulib-sql.h
 * @brief Focused SQL key/value statements over a shared memtable, not general
 * SQL. Define MULIB_IMPL_SQL plus MEMTABLE, STRBUF and BUFFER. See manual for
 * grammar. */
#ifndef MULIB_SQL_H
#define MULIB_SQL_H
#include "mulib-buffer.h"
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Execute SELECT value FROM kv WHERE key='k'; INSERT INTO kv VALUES
   * ('k','v'); DELETE FROM kv WHERE key='k'. Keywords case-insensitive; quotes
   * escape as ''. Optional single semicolon. INSERT upserts. No binary NUL
   * literals. SELECT appends raw value to result (required only for SELECT).
   * 0 success, 1 missing SELECT/DELETE, -1 syntax/invalid, other negative
   * storage/OOM. error_offset receives first unparsed byte for syntax,
   * SIZE_MAX otherwise. */
  int mulib_sql_execute (mulib_memtable_t *table, const char *sql,
                         size_t length, mulib_buffer_t *result,
                         size_t *error_offset);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SQL) && !defined(MULIB_SQL_IMPLEMENTED)
#define MULIB_SQL_IMPLEMENTED
#include "mulib-strbuf.h"
#include <string.h>
typedef struct mulib_sql_parser_
{
  const char *s;
  size_t n, p;
} mulib_sql_parser_;
static void
mulib_sql_space_ (mulib_sql_parser_ *p)
{
  while (p->p < p->n && strchr (" \t\r\n", p->s[p->p]))
    ++p->p;
}
static int
mulib_sql_word_ (mulib_sql_parser_ *p, const char *w)
{
  size_t i = 0, start;
  mulib_sql_space_ (p);
  start = p->p;
  while (w[i] && start + i < p->n)
    {
      char c = p->s[start + i];
      if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
      if (c != w[i])
        return 0;
      ++i;
    }
  if (w[i])
    return 0;
  if (start + i < p->n)
    {
      char c = p->s[start + i];
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
          || (c >= '0' && c <= '9') || c == '_')
        return 0;
    }
  p->p += i;
  return 1;
}
static int
mulib_sql_char_ (mulib_sql_parser_ *p, char c)
{
  mulib_sql_space_ (p);
  if (p->p == p->n || p->s[p->p] != c)
    return 0;
  ++p->p;
  return 1;
}
static int
mulib_sql_literal_ (mulib_sql_parser_ *p, mulib_strbuf_t *b)
{
  if (!mulib_sql_char_ (p, '\''))
    return 0;
  while (p->p < p->n)
    {
      char c = p->s[p->p++];
      if (c == '\'')
        {
          if (p->p == p->n || p->s[p->p] != '\'')
            return 1;
          ++p->p;
        }
      if (!mulib_strbuf_putc (b, c))
        return -1;
    }
  return 0;
}
int
mulib_sql_execute (mulib_memtable_t *t, const char *s, size_t n,
                   mulib_buffer_t *out, size_t *error)
{
  mulib_sql_parser_ p;
  mulib_strbuf_t k = { 0 }, v = { 0 };
  int op = 0, rc = -1, a;
  const mulib_memtable_record_t *r;
  if (error)
    *error = SIZE_MAX;
  if (!t || !s)
    return -1;
  p.s = s;
  p.n = n;
  p.p = 0;
  if (memchr (s, 0, n))
    goto syntax;
  if (mulib_sql_word_ (&p, "SELECT"))
    {
      op = 1;
      if (!out || !mulib_sql_word_ (&p, "VALUE")
          || !mulib_sql_word_ (&p, "FROM") || !mulib_sql_word_ (&p, "KV"))
        goto syntax;
    }
  else if (mulib_sql_word_ (&p, "INSERT"))
    {
      op = 2;
      if (!mulib_sql_word_ (&p, "INTO") || !mulib_sql_word_ (&p, "KV")
          || !mulib_sql_word_ (&p, "VALUES") || !mulib_sql_char_ (&p, '('))
        goto syntax;
    }
  else if (mulib_sql_word_ (&p, "DELETE"))
    {
      op = 3;
      if (!mulib_sql_word_ (&p, "FROM") || !mulib_sql_word_ (&p, "KV"))
        goto syntax;
    }
  else
    goto syntax;
  if (op != 2
      && (!mulib_sql_word_ (&p, "WHERE") || !mulib_sql_word_ (&p, "KEY")
          || !mulib_sql_char_ (&p, '=')))
    goto syntax;
  a = mulib_sql_literal_ (&p, &k);
  if (a < 0)
    {
      rc = -2;
      goto done;
    }
  if (!a)
    goto syntax;
  if (op == 2)
    {
      if (!mulib_sql_char_ (&p, ','))
        goto syntax;
      a = mulib_sql_literal_ (&p, &v);
      if (a < 0)
        {
          rc = -2;
          goto done;
        }
      if (!a || !mulib_sql_char_ (&p, ')'))
        goto syntax;
    }
  mulib_sql_space_ (&p);
  if (p.p < n && p.s[p.p] == ';')
    {
      ++p.p;
      mulib_sql_space_ (&p);
    }
  if (p.p != n)
    goto syntax;
  if (op == 2)
    {
      uint64_t sequence = 0;
      rc = mulib_memtable_get (t, k.data, k.length, &r);
      if (rc < 0)
        goto done;
      if (!rc)
        {
          if (r->sequence == UINT64_MAX)
            {
              rc = MULIB_MEMTABLE_EOVERFLOW;
              goto done;
            }
          sequence = r->sequence + 1;
        }
      rc = mulib_memtable_put (t, k.data, k.length, v.data, v.length, sequence,
                               0);
    }
  else
    {
      rc = mulib_memtable_get (t, k.data, k.length, &r);
      if (!rc && r->deleted)
        rc = 1;
      if (!rc)
        rc = op == 1
                 ? (mulib_buffer_append (out, r->value, r->value_size) ? 0
                                                                       : -2)
                 : mulib_memtable_put (t, k.data, k.length, NULL, 0,
                                       r->sequence, 1);
    }
  goto done;
syntax:
  if (error)
    *error = p.p;
done:
  mulib_strbuf_free (&k);
  mulib_strbuf_free (&v);
  return rc;
}
#endif
