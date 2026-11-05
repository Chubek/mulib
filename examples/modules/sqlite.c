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
#include "mulib-sqlite.h"
#include <assert.h>

int main(void) {
  mulib_sqlite_t *db=NULL;
  mulib_sqlite_stmt_t *stmt=NULL;
  mulib_sqlite_error_t error;
  const char *query="SELECT id, name, score + 2 AS answer FROM people ORDER BY id;";
  int rc=mulib_sqlite_open(":memory:",0,NULL,&db,&error);
  if (rc) fprintf(stderr,"open: %s\n",error.message);
  assert(!rc);
  rc=mulib_sqlite_exec(db,"CREATE TABLE people(id INTEGER PRIMARY KEY, name TEXT NOT NULL, score INTEGER);"
      "BEGIN; INSERT INTO people(name, score) VALUES('Ada',40),('Grace',98); COMMIT;",&error);
  if (rc) fprintf(stderr,"exec: %zu: %s\n",error.offset,error.message);
  assert(!rc);
  assert(!mulib_sqlite_prepare(db,query,strlen(query),&stmt,NULL,&error));
  while ((rc=mulib_sqlite_step(stmt))==MULIB_SQLITE_ROW) {
    printf("%" PRId64 " %s %" PRId64 "\n",mulib_sqlite_column(stmt,0)->integer,
      mulib_sqlite_column(stmt,1)->bytes,mulib_sqlite_column(stmt,2)->integer);
  }
  assert(rc==MULIB_SQLITE_DONE);
  mulib_sqlite_finalize(stmt); assert(!mulib_sqlite_close(db)); return 0;
}
