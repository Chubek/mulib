/**
 * @file mulib-termdb.h
 * @brief Explicit read-only connector for generated terminal code databases.
 *
 * Open only trusted databases: their templates may contain control sequences
 * with terminal-side effects. No function writes to a terminal. Only an
 * explicit terminal operation should request expansion and send those bytes;
 * never interpret ordinary display text or metadata as terminal operations.
 * There is no implicit path, environment, or terminfo database lookup.
 *
 * Version 1 requires termdb_meta("key" TEXT, value TEXT) with exactly one
 * schema_version row of value "1", and termdb_codes(name TEXT, profile TEXT,
 * template TEXT, arity INTEGER, description TEXT, source TEXT, line INTEGER).
 * Quote the key identifier in CREATE TABLE because KEY is reserved by the
 * bounded SQL parser. Extra tables are allowed.
 *
 * Define MULIB_IMPL_TERMDB and implement SQLITE, TERMINFO, STRBUF and their
 * dependencies. File access uses the SQLite pager (POSIX). No system SQLite
 * or terminfo library is needed.
 */
#ifndef MULIB_TERMDB_H
#define MULIB_TERMDB_H
#include "mulib-sqlite.h"
#include "mulib-terminfo.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Owned connection. Initialize to zero and do not copy or share
   * concurrently. Treat members as private. */
  typedef struct mulib_termdb
  {
    mulib_sqlite_t *connection;
  } mulib_termdb_t;

  /** @brief Open an explicit file read-only; validate the version-1 schema.
   * Return 1 on success, or 0 leaving db unchanged. Close before reopening. */
  int mulib_termdb_open (mulib_termdb_t *db, const char *path);

  /** @brief Close and zero the connection; 1 on success (also for NULL or an
   * already closed connection). Failure leaves db unchanged. */
  int mulib_termdb_close (mulib_termdb_t *db);

  /** @brief Append metadata by exact, case-sensitive key. Keys are 1..255
   * UTF-8 bytes and text values are at most 64 KiB, without embedded NULs.
   * Return 1 on success; 0 (including a missing key) leaves output unchanged. */
  int mulib_termdb_meta (mulib_termdb_t *db, const char *key,
                        mulib_strbuf_t *output);

  /** @brief Append a numeric tparm-subset expansion to output. Lookup is
   * exact and case-sensitive; name and profile are 1..255 UTF-8 bytes.
   * NULL profile means "ansi"; other profiles fall back to "ansi" only if
   * their row is absent. An invalid row never triggers fallback.
   * count must equal the row's arity (0..9); args may be NULL only if count
   * is zero. Templates must be ASCII without embedded NULs, at most 64 KiB.
   * Return 1 on success; 0 leaves output unchanged. */
  int mulib_termdb_emit (mulib_termdb_t *db, const char *profile,
                        const char *name, const int32_t *args, size_t count,
                        mulib_strbuf_t *output);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TERMDB_H */

#if defined(MULIB_IMPL_TERMDB) && !defined(MULIB_TERMDB_IMPLEMENTED)
#define MULIB_TERMDB_IMPLEMENTED
#include "mulib-utf8.h"
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  enum { MULIB_TERMDB_KEY_MAX_ = 255, MULIB_TERMDB_TEXT_MAX_ = 65536 };

  static int
  mulib_termdb_key_ (const char *key, size_t *length)
  {
    size_t n;
    if (!key)
      return 0;
    for (n = 0; n <= MULIB_TERMDB_KEY_MAX_ && key[n]; ++n)
      ;
    if (!n || n > MULIB_TERMDB_KEY_MAX_
        || !mulib_utf8_validate ((const unsigned char *)key, n))
      return 0;
    *length = n;
    return 1;
  }

  static int
  mulib_termdb_text_ (const mulib_sqlite_value_t *value)
  {
    return value && value->type == MULIB_SQLITE_TEXT && value->bytes
           && value->length <= MULIB_TERMDB_TEXT_MAX_
           && !memchr (value->bytes, 0, value->length)
           && mulib_utf8_validate (value->bytes, value->length);
  }

  static int
  mulib_termdb_text_is_ (const mulib_sqlite_value_t *value, const char *text,
                         int ignore_case)
  {
    size_t n = strlen (text), i;
    if (!value || value->type != MULIB_SQLITE_TEXT || !value->bytes
        || value->length != n)
      return 0;
    for (i = 0; i < n; ++i)
      {
        unsigned c = value->bytes[i];
        if (ignore_case && c >= 'a' && c <= 'z')
          c -= 'a' - 'A';
        if (c != (unsigned char)text[i])
          return 0;
      }
    return 1;
  }

  static int
  mulib_termdb_schema_ (mulib_sqlite_t *db, const char *sql,
                        const char *const names[], unsigned count,
                        unsigned integers)
  {
    mulib_sqlite_stmt_t *stmt = NULL;
    unsigned seen = 0;
    int rc;
    if (mulib_sqlite_prepare (db, sql, strlen (sql), &stmt, NULL, NULL)
        != MULIB_SQLITE_OK)
      return 0;
    while ((rc = mulib_sqlite_step (stmt)) == MULIB_SQLITE_ROW)
      {
        unsigned i;
        for (i = 0; i < count; ++i)
          if (mulib_termdb_text_is_ (mulib_sqlite_column (stmt, 1), names[i],
                                     0))
            break;
        if (i == count || (seen & (1u << i))
            || !mulib_termdb_text_is_ (mulib_sqlite_column (stmt, 2),
                                       integers & (1u << i) ? "INTEGER"
                                                             : "TEXT",
                                       1))
          break;
        seen |= 1u << i;
      }
    mulib_sqlite_finalize (stmt);
    return rc == MULIB_SQLITE_DONE && seen == (1u << count) - 1;
  }

  static mulib_sqlite_stmt_t *
  mulib_termdb_query_ (mulib_sqlite_t *db, const char *sql, const char *key,
                       size_t key_n, const char *profile, size_t profile_n)
  {
    mulib_sqlite_stmt_t *stmt = NULL;
    if (mulib_sqlite_prepare (db, sql, strlen (sql), &stmt, NULL, NULL)
            != MULIB_SQLITE_OK
        || mulib_sqlite_bind_text (stmt, 1, key, key_n) != MULIB_SQLITE_OK
        || (profile
            && mulib_sqlite_bind_text (stmt, 2, profile, profile_n)
                   != MULIB_SQLITE_OK))
      {
        mulib_sqlite_finalize (stmt);
        return NULL;
      }
    return stmt;
  }

  int
  mulib_termdb_meta (mulib_termdb_t *db, const char *key, mulib_strbuf_t *out)
  {
    static const char sql[]
        = "SELECT value FROM termdb_meta WHERE \"key\"=?1 LIMIT 2;";
    mulib_sqlite_stmt_t *stmt;
    size_t n;
    int ok = 0;
    if (!db || !db->connection || !out || !mulib_termdb_key_ (key, &n))
      return 0;
    stmt = mulib_termdb_query_ (db->connection, sql, key, n, NULL, 0);
    if (stmt && mulib_sqlite_step (stmt) == MULIB_SQLITE_ROW)
      {
        const mulib_sqlite_value_t *v = mulib_sqlite_column (stmt, 0);
        /* Borrowed column values live until reset or finalize. */
        if (mulib_sqlite_step (stmt) == MULIB_SQLITE_DONE
            && mulib_termdb_text_ (v))
          ok = mulib_strbuf_append_n (out, (const char *)v->bytes, v->length);
      }
    mulib_sqlite_finalize (stmt);
    return ok;
  }

  int
  mulib_termdb_open (mulib_termdb_t *db, const char *path)
  {
    static const char meta_info[] = "PRAGMA table_info(termdb_meta);";
    static const char codes_info[] = "PRAGMA table_info(termdb_codes);";
    static const char *const meta[] = { "key", "value" };
    static const char *const codes[]
        = { "name", "profile", "template", "arity", "description", "source",
            "line" };
    mulib_termdb_t next = { NULL };
    mulib_strbuf_t version = { NULL, 0, 0 };
    int ok;
    if (!db || db->connection || !path || !*path || !strcmp (path, ":memory:")
        || mulib_sqlite_open (path, MULIB_SQLITE_OPEN_READONLY, NULL,
                             &next.connection, NULL) != MULIB_SQLITE_OK)
      return 0;
    ok = mulib_termdb_schema_ (next.connection, meta_info, meta, 2, 0)
         && mulib_termdb_schema_ (next.connection, codes_info, codes, 7,
                                  (1u << 3) | (1u << 6))
         && mulib_termdb_meta (&next, "schema_version", &version)
         && version.length == 1 && version.data[0] == '1';
    mulib_strbuf_free (&version);
    if (ok)
      *db = next;
    else
      mulib_termdb_close (&next);
    return ok;
  }

  int
  mulib_termdb_close (mulib_termdb_t *db)
  {
    if (!db)
      return 1;
    if (mulib_sqlite_close (db->connection) != MULIB_SQLITE_OK)
      return 0;
    db->connection = NULL;
    return 1;
  }

  static int
  mulib_termdb_template_ (const mulib_sqlite_value_t *v, size_t arity)
  {
    size_t i;
    if (!mulib_termdb_text_ (v))
      return 0;
    for (i = 0; i < v->length; ++i)
      if (v->bytes[i] > 127)
        return 0;
    /* The expander checks syntax. Only check parameter references here, so
     * a malformed arity cannot turn a missing argument into an implicit zero. */
    for (i = 0; i + 1 < v->length; ++i)
      if (v->bytes[i] == '%')
        {
          unsigned op = v->bytes[++i];
          if (op == '\'' && i + 2 < v->length)
            i += 2;
          else if (op == 'p' && i + 1 < v->length
                   && v->bytes[i + 1] > '0' + arity)
            return 0;
        }
    return 1;
  }

  int
  mulib_termdb_emit (mulib_termdb_t *db, const char *profile, const char *name,
                     const int32_t *args, size_t count, mulib_strbuf_t *out)
  {
    static const char sql[]
        = "SELECT template, arity, description, source, line FROM termdb_codes "
          "WHERE name=?1 AND profile=?2 LIMIT 2;";
    int32_t params[9] = { 0 };
    mulib_sqlite_stmt_t *stmt;
    size_t name_n, profile_n;
    int rc, ok = 0;
    if (!profile)
      profile = "ansi";
    if (!db || !db->connection || !out || count > 9 || (count && !args)
        || !mulib_termdb_key_ (name, &name_n)
        || !mulib_termdb_key_ (profile, &profile_n))
      return 0;
    stmt = mulib_termdb_query_ (db->connection, sql, name, name_n, profile,
                                profile_n);
    rc = mulib_sqlite_step (stmt);
    if (rc == MULIB_SQLITE_DONE && strcmp (profile, "ansi"))
      {
        mulib_sqlite_finalize (stmt);
        stmt = mulib_termdb_query_ (db->connection, sql, name, name_n, "ansi",
                                    4);
        rc = mulib_sqlite_step (stmt);
      }
    if (rc == MULIB_SQLITE_ROW)
      {
        const mulib_sqlite_value_t *format = mulib_sqlite_column (stmt, 0);
        const mulib_sqlite_value_t *arity = mulib_sqlite_column (stmt, 1);
        const mulib_sqlite_value_t *line = mulib_sqlite_column (stmt, 4);
        if (arity->type == MULIB_SQLITE_INTEGER
            && arity->integer == (int64_t)count
            && line->type == MULIB_SQLITE_INTEGER && line->integer >= 0
            && mulib_termdb_text_ (mulib_sqlite_column (stmt, 2))
            && mulib_termdb_text_ (mulib_sqlite_column (stmt, 3))
            && mulib_termdb_template_ (format, count)
            && mulib_sqlite_step (stmt) == MULIB_SQLITE_DONE)
          {
            if (count)
              memcpy (params, args, count * sizeof (*args));
            ok = mulib_terminfo_expand ((const char *)format->bytes, params,
                                        out);
          }
      }
    mulib_sqlite_finalize (stmt);
    return ok;
  }

#ifdef __cplusplus
}
#endif
#endif /* implementation */
