#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_RNG
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SQLITE_FORMAT
#define MULIB_IMPL_SQLITE_PAGER
#define MULIB_IMPL_SQLITE
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMDB
#include "mulib-termdb.h"
#undef NDEBUG
#include <assert.h>

/* Print only the requested value for cross-tool tests with a generated file. */
static int
read_generated(int argc, char **argv)
{
  mulib_termdb_t db = { NULL };
  mulib_strbuf_t out = { NULL, 0, 0 };
  int ok = 0;
  if (argc < 3 || !mulib_termdb_open(&db, argv[2]))
    return 1;
  if (!strcmp(argv[1], "--meta") && argc == 4)
    ok = mulib_termdb_meta(&db, argv[3], &out);
  if (!strcmp(argv[1], "--emit") && argc >= 5 && argc <= 14)
    {
      const char *profile = strcmp(argv[3], "-") ? argv[3] : NULL;
      int32_t args[9];
      int i;
      for (i = 5; i < argc; ++i)
        {
          char *end;
          intmax_t n;
          errno = 0;
          n = strtoimax(argv[i], &end, 10);
          if (errno || end == argv[i] || *end || n < INT32_MIN || n > INT32_MAX)
            break;
          args[i - 5] = (int32_t)n;
        }
      if (i == argc)
        ok = mulib_termdb_emit(&db, profile, argv[4], args, (size_t)(i - 5), &out);
    }
  if (ok && out.length)
    ok = fwrite(out.data, 1, out.length, stdout) == out.length;
  mulib_strbuf_free(&out);
  return mulib_termdb_close(&db) && ok ? 0 : 1;
}

#if defined(__unix__) || defined(__APPLE__)
static const char meta_schema[] =
    "CREATE TABLE termdb_meta(\"key\" TEXT,value TEXT);";
static const char codes_schema[] =
    "CREATE TABLE termdb_codes(name TEXT,profile TEXT,template TEXT,arity INTEGER,"
    "description TEXT,source TEXT,line INTEGER);";
static const char rows[] =
    "INSERT INTO termdb_meta VALUES('schema_version','1'),('fixture','local SQL'),"
    "('empty',''),('dup','one'),('dup','two'),('bad',x'31');"
    "INSERT INTO termdb_codes VALUES"
    "('Move','ansi','\033[%i%p1%d;%p2%dH',2,'Cursor position','fixture',1),"
    "('move','ansi','\033[H',0,'Case-sensitive','fixture',2),"
    "('Move','xterm','\033[%p1%d;%p2%df',2,'Override','fixture',3),"
    "('highlight','xterm','\033[1m',0,'Extended only','fixture',4),"
    "('Move','bad','not selected',0,'Wrong arity','fixture',5),"
    "('Move','dup','duplicate one',2,'Duplicate','fixture',6),"
    "('Move','dup','duplicate two',2,'Duplicate','fixture',7),"
    "('Move','blob',x'1b5b48',2,'Blob template','fixture',8),"
    "('Move','unicode','\xc3\xa9',2,'Non-ASCII template','fixture',9),"
    "('Move','syntax','partial%q',2,'Unsupported template','fixture',10),"
    "('Move','arity','%p3%d',2,'Missing parameter','fixture',11),"
    "('Move','type','bad','two','Wrong arity type','fixture',12),"
    "('Move','line','bad',2,'Bad source line','fixture',-1),"
    "('Move','null','bad',2,NULL,'fixture',14),"
    "('literal'' OR 1 --','ansi','%''%''%c%%p9',0,'Percent literal','fixture',15),"
    "('ninth','ansi','%p9%03d',9,'Ninth parameter','fixture',16),"
    "('no-op','ansi','',0,'Empty template','fixture',17);";

static void
exec_ok(mulib_sqlite_t *db, const char *sql)
{
  mulib_sqlite_error_t err;
  int rc = mulib_sqlite_exec(db, sql, &err);
  if (rc != MULIB_SQLITE_OK)
    fprintf(stderr, "%s: SQLite error %d at %zu: %s\n",
            sql, rc, err.offset, err.message);
  assert(rc == MULIB_SQLITE_OK);
}

static void
binary_row(mulib_sqlite_t *db, const char *name, const char *bytes, size_t n,
           int64_t arity)
{
  static const char sql[] =
      "INSERT INTO termdb_codes VALUES(?1,'ansi',?2,?3,'','fixture',18);";
  mulib_sqlite_stmt_t *stmt = NULL;
  assert(mulib_sqlite_prepare(db, sql, sizeof sql - 1, &stmt, NULL, NULL)
         == MULIB_SQLITE_OK);
  assert(mulib_sqlite_bind_text(stmt, 1, name, strlen(name)) == MULIB_SQLITE_OK);
  assert(mulib_sqlite_bind_text(stmt, 2, bytes, n) == MULIB_SQLITE_OK);
  assert(mulib_sqlite_bind_int64(stmt, 3, arity) == MULIB_SQLITE_OK);
  assert(mulib_sqlite_step(stmt) == MULIB_SQLITE_DONE);
  mulib_sqlite_finalize(stmt);
}

static void
test_open_failures(const char *path)
{
  static const char *const changes[] = {
    "INSERT INTO termdb_meta VALUES('Schema_version','1');",
    "INSERT INTO termdb_meta VALUES('schema_version','2');",
    "INSERT INTO termdb_meta VALUES('schema_version',x'31');",
    "INSERT INTO termdb_meta VALUES('schema_version','1'),('schema_version','1');",
    "INSERT INTO termdb_meta VALUES('schema_version','1');"
      "DROP TABLE termdb_codes;",
    "INSERT INTO termdb_meta VALUES('schema_version','1');"
      "DROP TABLE termdb_codes; CREATE TABLE termdb_codes(name TEXT);",
    "INSERT INTO termdb_meta VALUES('schema_version','1');"
      "DROP TABLE termdb_codes;"
      "CREATE TABLE termdb_codes(name TEXT,profile TEXT,template TEXT,"
      "arity TEXT,description TEXT,source TEXT,line INTEGER);",
  };
  mulib_termdb_t db = { NULL };
  size_t i;
  assert(!mulib_termdb_open(NULL, path));
  assert(!mulib_termdb_open(&db, NULL));
  assert(!mulib_termdb_open(&db, ""));
  assert(!mulib_termdb_open(&db, ":memory:"));
  assert(!mulib_termdb_open(&db, path));
  assert(access(path, F_OK) != 0);
  for (i = 0; i < sizeof changes / sizeof changes[0]; ++i)
    {
      mulib_sqlite_t *sql = NULL;
      assert(mulib_sqlite_open(path, MULIB_SQLITE_OPEN_CREATE, NULL, &sql, NULL)
             == MULIB_SQLITE_OK);
      exec_ok(sql, meta_schema);
      exec_ok(sql, codes_schema);
      exec_ok(sql, changes[i]);
      assert(mulib_sqlite_close(sql) == MULIB_SQLITE_OK);
      assert(!mulib_termdb_open(&db, path));
      assert(db.connection == NULL);
      assert(unlink(path) == 0);
    }
}

static void
test_local(const char *path)
{
  static const char wide[] = "%p1%032d";
  static const char *const bad_profiles[] = {
    "bad", "dup", "blob", "unicode", "syntax", "arity", "type", "line", "null",
  };
  mulib_sqlite_t *sql = NULL;
  mulib_termdb_t db = { NULL };
  mulib_strbuf_t out = { NULL, 0, 0 };
  char key[257], *large, *old;
  int32_t args[9] = { 1, 3, 0, 0, 0, 0, 0, 0, 7 };
  size_t i, before, capacity;
  assert(mulib_sqlite_open(path, MULIB_SQLITE_OPEN_CREATE, NULL, &sql, NULL)
         == MULIB_SQLITE_OK);
  exec_ok(sql, meta_schema);
  exec_ok(sql, codes_schema);
  exec_ok(sql, rows);
  binary_row(sql, "embedded", "x\0y", 3, 0);
  large = malloc(65537);
  assert(large);
  memset(large, 'x', 65537);
  binary_row(sql, "large", large, 65537, 0);
  for (i = 0; i < 2049; ++i)
    memcpy(large + i * (sizeof wide - 1), wide, sizeof wide - 1);
  binary_row(sql, "large-output", large, i * (sizeof wide - 1), 1);
  free(large);
  assert(mulib_sqlite_close(sql) == MULIB_SQLITE_OK);
  assert(chmod(path, 0444) == 0);

  assert(!mulib_termdb_emit(&db, NULL, "Move", args, 2, &out));
  assert(mulib_termdb_open(&db, path));
  assert(!mulib_termdb_open(&db, path));
  assert(mulib_termdb_meta(&db, "fixture", &out));
  assert(mulib_termdb_meta(&db, "empty", &out));
  assert(!strcmp(out.data, "local SQL"));
  mulib_strbuf_free(&out);

  assert(mulib_strbuf_append(&out, "prefix:"));
  assert(mulib_termdb_emit(&db, NULL, "Move", args, 2, &out));
  assert(mulib_termdb_emit(&db, "xterm", "Move", args, 2, &out));
  assert(mulib_termdb_emit(&db, "XTerm", "Move", args, 2, &out));
  assert(mulib_termdb_emit(&db, "xterm", "highlight", NULL, 0, &out));
  assert(mulib_termdb_emit(&db, NULL, "move", NULL, 0, &out));
  assert(mulib_termdb_emit(&db, NULL, "literal' OR 1 --", NULL, 0, &out));
  assert(mulib_termdb_emit(&db, NULL, "ninth", args, 9, &out));
  assert(mulib_termdb_emit(&db, NULL, "no-op", NULL, 0, &out));
  assert(!strcmp(out.data, "prefix:\033[2;4H\033[1;3f\033[2;4H\033[1m\033[H"
                           "%%p9007"));

  old = out.data;
  before = out.length;
  capacity = out.capacity;
  assert(!mulib_termdb_meta(&db, "Fixture", &out));
  assert(!mulib_termdb_meta(&db, "' OR 1 --", &out));
  assert(!mulib_termdb_meta(&db, "dup", &out));
  assert(!mulib_termdb_meta(&db, "bad", &out));
  for (i = 0; i < sizeof bad_profiles / sizeof bad_profiles[0]; ++i)
    assert(!mulib_termdb_emit(&db, bad_profiles[i], "Move", args, 2, &out));
  assert(!mulib_termdb_emit(&db, NULL, "highlight", NULL, 0, &out));
  assert(!mulib_termdb_emit(&db, "XTerm", "highlight", NULL, 0, &out));
  assert(!mulib_termdb_emit(&db, NULL, "MOVE", NULL, 0, &out));
  assert(!mulib_termdb_emit(&db, NULL, "x' OR 1 --", NULL, 0, &out));
  assert(!mulib_termdb_emit(&db, "' OR 1 --", "highlight", NULL, 0, &out));
  assert(!mulib_termdb_emit(&db, NULL, "embedded", NULL, 0, &out));
  assert(!mulib_termdb_emit(&db, NULL, "large", NULL, 0, &out));
  assert(!mulib_termdb_emit(&db, NULL, "large-output", args, 1, &out));
  assert(!mulib_termdb_emit(&db, "", "Move", args, 2, &out));
  assert(!mulib_termdb_emit(&db, NULL, "", args, 2, &out));
  assert(!mulib_termdb_emit(&db, NULL, "Move", args, 1, &out));
  assert(!mulib_termdb_emit(&db, NULL, "Move", args, 10, &out));
  assert(!mulib_termdb_emit(&db, NULL, "Move", NULL, 2, &out));
  assert(!mulib_termdb_emit(&db, NULL, "Move", args, 2, NULL));
  memset(key, 'x', sizeof key - 1);
  key[sizeof key - 1] = '\0';
  assert(!mulib_termdb_emit(&db, NULL, key, NULL, 0, &out));
  args[0] = INT32_MAX;
  assert(!mulib_termdb_emit(&db, NULL, "Move", args, 2, &out));
  assert(out.data == old && out.length == before && out.capacity == capacity);
  assert(!strcmp(out.data, "prefix:\033[2;4H\033[1;3f\033[2;4H\033[1m\033[H"
                           "%%p9007"));

  assert(mulib_termdb_close(&db));
  assert(mulib_termdb_close(&db));
  assert(mulib_termdb_close(NULL));
  assert(!mulib_termdb_meta(&db, "fixture", &out));
  assert(!mulib_termdb_emit(&db, NULL, "move", NULL, 0, &out));
  assert(mulib_termdb_open(&db, path));
  assert(mulib_termdb_close(&db));
  mulib_strbuf_free(&out);
  assert(unlink(path) == 0);
}
#endif

int
main(int argc, char **argv)
{
  if (argc > 1)
    return read_generated(argc, argv);
#if defined(__unix__) || defined(__APPLE__)
  {
    char dir[] = "mulib-termdb-XXXXXX";
    char path[sizeof dir + sizeof "/test.sqlite"];
    assert(mkdtemp(dir));
    assert(snprintf(path, sizeof path, "%s/test.sqlite", dir) > 0);
    test_open_failures(path);
    test_local(path);
    assert(rmdir(dir) == 0);
  }
  puts("TermDB tests passed");
#else
  puts("TermDB file tests require a POSIX SQLite pager");
#endif
  return 0;
}
