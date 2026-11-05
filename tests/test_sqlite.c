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

static mulib_sqlite_t *db;
static void exec_ok(const char *sql) {
  mulib_sqlite_error_t error; int rc=mulib_sqlite_exec(db,sql,&error);
  if (rc) fprintf(stderr,"%s: %d at %zu: %s\n",sql,rc,error.offset,error.message);
  assert(!rc);
}
static mulib_sqlite_stmt_t *query(const char *sql) {
  mulib_sqlite_stmt_t *s=NULL; mulib_sqlite_error_t error;
  int rc=mulib_sqlite_prepare(db,sql,strlen(sql),&s,NULL,&error);
  if (rc) fprintf(stderr,"prepare %s: %d at %zu: %s\n",sql,rc,error.offset,error.message);
  assert(!rc && s);
  rc=mulib_sqlite_step(s);
  if (rc!=MULIB_SQLITE_ROW) fprintf(stderr,"step %s: %d: %s\n",sql,rc,mulib_sqlite_error(db)->message);
  assert(rc==MULIB_SQLITE_ROW); return s;
}
static void integer(const char *sql,int64_t n) {
  mulib_sqlite_stmt_t *s=query(sql); const mulib_sqlite_value_t *v=mulib_sqlite_column(s,0);
  if (v->type!=MULIB_SQLITE_INTEGER || v->integer!=n) fprintf(stderr,"%s: type %d int %" PRId64 " real %.17g expected %" PRId64 "\n",sql,v->type,v->integer,v->real,n);
  assert(v->type==MULIB_SQLITE_INTEGER && v->integer==n); mulib_sqlite_finalize(s);
}
static void test_sql(void) {
  mulib_sqlite_stmt_t *s; const mulib_sqlite_value_t *v;
  integer("SELECT 2+3*4;",14); integer("SELECT -9223372036854775808;",INT64_MIN);
  integer("SELECT 9223372036854775807;",INT64_MAX);
  integer("SELECT 0xffffffffffffffff;",-1);
  integer("SELECT NOT 0;",1); integer("SELECT 1 AND NULL IS NULL;",1);
  integer("SELECT NULL IS NULL;",1); integer("SELECT 1 IS NOT NULL;",1);
  integer("SELECT 1 IS /* not an operator */ 1;",1);
  integer("SELECT 9007199254740993 > 9007199254740992.0;",1);
  integer("SELECT 'Ada' LIKE 'a%';",1);
  integer("SELECT 'é' LIKE '_';",1); integer("SELECT 'hello🌍' LIKE '%🌍';",1);
  integer("SELECT 1--comment\n+2;",3);
  s=query("SELECT 1/0, '' || 'a''b', x'0001ff', typeof(1), coalesce(NULL,42);");
  assert(mulib_sqlite_column(s,0)->type==MULIB_SQLITE_NULL);
  v=mulib_sqlite_column(s,1); assert(v->type==MULIB_SQLITE_TEXT && v->length==3 && !memcmp(v->bytes,"a'b",3));
  v=mulib_sqlite_column(s,2); assert(v->type==MULIB_SQLITE_BLOB && v->length==3 && v->bytes[2]==255);
  assert(mulib_sqlite_column(s,4)->integer==42); mulib_sqlite_finalize(s);
  exec_ok("CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT NOT NULL, amount INTEGER DEFAULT 7, raw BLOB);"
      "INSERT INTO t(name) VALUES('Ada'),('Grace');");
  assert(mulib_sqlite_changes(db)==2 && mulib_sqlite_last_insert_rowid(db)==2);
  integer("SELECT count(*) FROM t;",2); integer("SELECT sum(amount) FROM t;",14);
  s=query("SELECT name,max(id) FROM t;");
  assert(!strcmp((const char *)mulib_sqlite_column(s,0)->bytes,"Grace")); mulib_sqlite_finalize(s);
  exec_ok("UPDATE t SET amount=amount+1 WHERE id=1;"); assert(mulib_sqlite_changes(db)==1);
  integer("SELECT amount FROM t WHERE name='Ada';",8);
  s=query("SELECT name, amount AS a FROM t ORDER BY a DESC LIMIT 1;");
  assert(!strcmp((const char *)mulib_sqlite_column(s,0)->bytes,"Ada")); mulib_sqlite_finalize(s);
  assert(mulib_sqlite_exec(db,"INSERT INTO t(id,name) VALUES(3,'Third'),(1,'Duplicate');",NULL)==MULIB_SQLITE_CONSTRAINT);
  integer("SELECT count(*) FROM t;",2);
  assert(mulib_sqlite_exec(db,"UPDATE t SET name=NULL;",NULL)==MULIB_SQLITE_CONSTRAINT);
  integer("SELECT count(*) FROM t WHERE name IS NOT NULL;",2);
  assert(mulib_sqlite_exec(db,"CREATE TABLE unsupported(x UNIQUE);",NULL)==MULIB_SQLITE_UNSUPPORTED);
  exec_ok("BEGIN; INSERT INTO t(name) VALUES('Temporary'); SAVEPOINT innerone; DELETE FROM t WHERE id=1;"
    "ROLLBACK TO innerone; RELEASE innerone;");
  integer("SELECT count(*) FROM t;",3); exec_ok("ROLLBACK;"); integer("SELECT count(*) FROM t;",2);
  assert(mulib_sqlite_autocommit(db));
  exec_ok("SAVEPOINT outerone; INSERT INTO t(name) VALUES('Committed'); RELEASE outerone;");
  assert(mulib_sqlite_autocommit(db)); integer("SELECT count(*) FROM t;",3);
  exec_ok("DELETE FROM t WHERE id=3; DROP TABLE IF EXISTS missing; CREATE TABLE IF NOT EXISTS t(x);");
  s=query("SELECT name FROM sqlite_schema WHERE type='table';");
  assert(!strcmp((const char *)mulib_sqlite_column(s,0)->bytes,"t")); mulib_sqlite_finalize(s);
  exec_ok("PRAGMA user_version=42;"); integer("PRAGMA user_version;",42);
  assert(mulib_sqlite_exec(db,"SELECT unknown FROM t WHERE 0;",NULL)==MULIB_SQLITE_ERROR);
  assert(mulib_sqlite_exec(db,"SELECT unknown_function(1) FROM t WHERE 0;",NULL)==MULIB_SQLITE_UNSUPPORTED);
  assert(mulib_sqlite_exec(db,"SELECT 1; nonsense;",NULL)==MULIB_SQLITE_ERROR);
  exec_ok("CREATE TABLE empty(x); CREATE TABLE neg(id INTEGER PRIMARY KEY); INSERT INTO neg VALUES(-2); INSERT INTO neg DEFAULT VALUES;");
  integer("SELECT max(id) FROM neg;",-1);
  integer("SELECT count(*) FROM empty;",0);
  s=query("SELECT x, sum(x), avg(x), total(x) FROM empty;");
  assert(mulib_sqlite_column(s,0)->type==MULIB_SQLITE_NULL && mulib_sqlite_column(s,1)->type==MULIB_SQLITE_NULL);
  assert(mulib_sqlite_column(s,2)->type==MULIB_SQLITE_NULL && mulib_sqlite_column(s,3)->real==0); mulib_sqlite_finalize(s);
  assert(mulib_sqlite_exec(db,"SELECT x FROM empty ORDER BY missing;",NULL)==MULIB_SQLITE_ERROR);
  assert(mulib_sqlite_exec(db,"SELECT abs() FROM empty;",NULL)==MULIB_SQLITE_ERROR);
  assert(mulib_sqlite_exec(db,"CREATE TABLE parameter_default(x DEFAULT ?1);",NULL)==MULIB_SQLITE_UNSUPPORTED);
  exec_ok("DROP TABLE empty; DROP TABLE neg;");
  exec_ok("CREATE TABLE [a[[b](x); INSERT INTO [a[[b] VALUES(42);");
  integer("SELECT x FROM [a[[b];",42); exec_ok("DROP TABLE [a[[b];");
}
static void test_bindings(void) {
  const char *sql="INSERT INTO t(id,name,raw) VALUES(?3,:name,@raw); SELECT ?100;";
  mulib_sqlite_stmt_t *s=NULL; size_t tail=0;
  assert(!mulib_sqlite_prepare(db,sql,strlen(sql),&s,&tail,NULL) && s && tail<strlen(sql));
  assert(mulib_sqlite_parameter_count(s)==5);
  assert(mulib_sqlite_parameter_index(s,"?3")==3 && mulib_sqlite_parameter_index(s,":name")==4);
  assert(!mulib_sqlite_bind_int64(s,3,INT64_C(9007199254740993)));
  assert(!mulib_sqlite_bind_text(s,4,"a\0b",3)); assert(!mulib_sqlite_bind_blob(s,5,"\0\xff",2));
  assert(mulib_sqlite_close(db)==MULIB_SQLITE_BUSY);
  assert(mulib_sqlite_step(s)==MULIB_SQLITE_DONE);
  assert(mulib_sqlite_bind_int64(s,3,9)==MULIB_SQLITE_MISUSE);
  assert(!mulib_sqlite_reset(s)); assert(!mulib_sqlite_bind_int64(s,3,9));
  assert(mulib_sqlite_step(s)==MULIB_SQLITE_DONE); mulib_sqlite_finalize(s);
  s=query("SELECT id,name,raw FROM t WHERE id=9007199254740993;");
  assert(mulib_sqlite_column(s,0)->integer==INT64_C(9007199254740993));
  assert(mulib_sqlite_column(s,1)->length==3 && mulib_sqlite_column(s,1)->bytes[1]==0);
  assert(mulib_sqlite_column(s,2)->length==2 && mulib_sqlite_column(s,2)->bytes[1]==255); mulib_sqlite_finalize(s);
  sql="SELECT :same, :same, ?1, ?;";
  assert(!mulib_sqlite_prepare(db,sql,strlen(sql),&s,NULL,NULL));
  assert(mulib_sqlite_parameter_count(s)==2);
  assert(mulib_sqlite_parameter_index(s,":same")==1);
  assert(!mulib_sqlite_bind_int64(s,1,42) && !mulib_sqlite_bind_int64(s,2,7));
  assert(mulib_sqlite_step(s)==MULIB_SQLITE_ROW && mulib_sqlite_column(s,1)->integer==42 && mulib_sqlite_column(s,3)->integer==7);
  assert(!mulib_sqlite_reset(s) && !mulib_sqlite_clear_bindings(s)); mulib_sqlite_finalize(s);
}
static void test_limits(void) {
  mulib_sqlite_t *limited=NULL; mulib_sqlite_options_t options={0};
  options.max_rows=2;
  assert(!mulib_sqlite_open(":memory:",0,&options,&limited,NULL));
  assert(!mulib_sqlite_exec(limited,"CREATE TABLE small(x);",NULL));
  assert(mulib_sqlite_exec(limited,"INSERT INTO small VALUES(1),(2),(3);",NULL)==MULIB_SQLITE_TOOBIG);
  assert(!mulib_sqlite_exec(limited,"INSERT INTO small VALUES(42);",NULL));
  assert(mulib_sqlite_changes(limited)==1);
  assert(!mulib_sqlite_close(limited));
  memset(&options,0,sizeof options); options.max_sql_bytes=4;
  assert(!mulib_sqlite_open(":memory:",0,&options,&limited,NULL));
  assert(mulib_sqlite_exec(limited,"SELECT 1;",NULL)==MULIB_SQLITE_TOOBIG); assert(!mulib_sqlite_close(limited));
  memset(&options,0,sizeof options); options.max_steps=1;
  assert(!mulib_sqlite_open(":memory:",0,&options,&limited,NULL));
  assert(mulib_sqlite_exec(limited,"SELECT 1;",NULL)==MULIB_SQLITE_TOOBIG); assert(!mulib_sqlite_close(limited));
}
int main(int argc,char **argv) {
  mulib_sqlite_error_t error; int rc;
  if (argc==4 && (!strcmp(argv[1],"--query") || !strcmp(argv[1],"--readonly"))) {
    mulib_sqlite_stmt_t *s=NULL;
    rc=mulib_sqlite_open(argv[2],!strcmp(argv[1],"--readonly")?MULIB_SQLITE_OPEN_READONLY:0,NULL,&db,&error);
    if (!rc) rc=mulib_sqlite_prepare(db,argv[3],strlen(argv[3]),&s,NULL,&error);
    if (!rc && s) {
      while ((rc=mulib_sqlite_step(s))==MULIB_SQLITE_ROW) {
        size_t i,j;
        for (i=0;i<mulib_sqlite_column_count(s);++i) {
          const mulib_sqlite_value_t *v=mulib_sqlite_column(s,i);
          if (i) putchar('\t');
          if (v->type==MULIB_SQLITE_NULL) putchar('N');
          else if (v->type==MULIB_SQLITE_INTEGER) printf("I%" PRId64,v->integer);
          else if (v->type==MULIB_SQLITE_REAL) printf("R%.17g",v->real);
          else { putchar(v->type==MULIB_SQLITE_TEXT?'T':'B'); for (j=0;j<v->length;++j) printf("%02x",v->bytes[j]); }
        }
        putchar('\n');
      }
      if (rc==MULIB_SQLITE_DONE) rc=0;
      if (rc) error=*mulib_sqlite_error(db);
    }
    if (rc) fprintf(stderr,"%d: %s\n",rc,error.message);
    mulib_sqlite_finalize(s); mulib_sqlite_close(db); return rc?1:0;
  }
  if (argc==3 && !strcmp(argv[1],"--hold")) {
    rc=mulib_sqlite_open(argv[2],0,NULL,&db,&error);
    if (rc) return 1;
    puts("locked"); fflush(stdout); (void)getchar(); mulib_sqlite_close(db); return 0;
  }
  if (argc==4 && !strcmp(argv[1],"--exec")) {
    rc=mulib_sqlite_open(argv[2],MULIB_SQLITE_OPEN_CREATE,NULL,&db,&error);
    if (!rc) rc=mulib_sqlite_exec(db,argv[3],&error);
    if (rc) fprintf(stderr,"%d: %s\n",rc,error.message);
    mulib_sqlite_close(db); return rc?1:0;
  }
  assert(!mulib_sqlite_open(":memory:",0,NULL,&db,&error));
  test_sql(); test_bindings(); assert(!mulib_sqlite_close(db)); test_limits();
  puts("SQLite SQL and memory transaction tests passed"); return 0;
}
