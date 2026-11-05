/**
 * @file mulib-sqlite.h
 * @brief Independent SQLite-file database with a parsec SQL execution engine.
 * Define MULIB_IMPL_SQLITE; explicitly implement SQLITE_FORMAT, SQLITE_PAGER,
 * PARSEC, PEG, ARENA, BUFFER, ENDIAN, RNG, STRBUF and UTF8. No SQLite dependency.
 * This is an implemented, bounded SQL profile, not yet full SQLite compatibility.
 * Unsupported syntax/storage features fail; see manual/mulib-sqlite.md.
 */
#ifndef MULIB_SQLITE_H
#define MULIB_SQLITE_H
#include "mulib-sqlite-pager.h"
#include "mulib-parsec.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sqlite mulib_sqlite_t;
typedef struct mulib_sqlite_stmt mulib_sqlite_stmt_t;
/** Owned diagnostic; offsets refer to the SQL bytes (SIZE_MAX if unavailable). */
typedef struct mulib_sqlite_error { int code; size_t offset; char message[256]; } mulib_sqlite_error_t;
/** Zero fields select 64 MiB file/payload, 1 MiB SQL, 100000 rows per table or
 * result, 2000 columns, 10000000 evaluation steps and default parsec limits. */
typedef struct mulib_sqlite_options {
  size_t max_bytes,max_sql_bytes,max_rows,max_columns,max_steps;
  mulib_parsec_options_t parser;
} mulib_sqlite_options_t;
/** Open a database, or :memory:. Flags are SQLITE_PAGER open flags. Output is
 * unchanged on failure. Native file locks last until close. */
int mulib_sqlite_open(const char *, unsigned, const mulib_sqlite_options_t *, mulib_sqlite_t **, mulib_sqlite_error_t *);
/** Close, discarding an uncommitted transaction. BUSY leaves connection alive
 * if statements still exist. NULL is harmless. */
int mulib_sqlite_close(mulib_sqlite_t *);
/** Prepare one statement. Copies SQL/AST, does not execute it. tail receives
 * the first byte after the statement. Empty input succeeds with *out=NULL. */
int mulib_sqlite_prepare(mulib_sqlite_t *,const char *,size_t,mulib_sqlite_stmt_t **,size_t *tail,mulib_sqlite_error_t *);
/** Number of parameter slots; ?NNN can create unused gaps. */
size_t mulib_sqlite_parameter_count(const mulib_sqlite_stmt_t *);
/** One-based slot for exact :name/@name/$name/?NNN spelling; zero if absent. */
size_t mulib_sqlite_parameter_index(const mulib_sqlite_stmt_t *,const char *);
/** Bind a copied value into a one-based slot. Only allowed before first step
 * or after reset. Unbound parameters have SQL NULL values. */
int mulib_sqlite_bind(mulib_sqlite_stmt_t *,size_t,const mulib_sqlite_value_t *);
/** Bind a signed integer without conversion through double. */
int mulib_sqlite_bind_int64(mulib_sqlite_stmt_t *,size_t,int64_t);
/** Bind copied UTF-8 text (embedded NUL allowed). */
int mulib_sqlite_bind_text(mulib_sqlite_stmt_t *,size_t,const char *,size_t);
/** Bind copied binary data, including an empty non-NULL BLOB. */
int mulib_sqlite_bind_blob(mulib_sqlite_stmt_t *,size_t,const void *,size_t);
/** Execute on first step and materialize a bounded result. Return ROW, DONE,
 * or an error. Errors remain sticky until reset. */
int mulib_sqlite_step(mulib_sqlite_stmt_t *);
/** Result column count, available after the first step, including empty results. */
size_t mulib_sqlite_column_count(const mulib_sqlite_stmt_t *);
/** Borrow a result column name, or NULL for an invalid index. */
const char *mulib_sqlite_column_name(const mulib_sqlite_stmt_t *,size_t);
/** Borrow the current row's typed value until reset/finalize. NULL if no row. */
const mulib_sqlite_value_t *mulib_sqlite_column(const mulib_sqlite_stmt_t *,size_t);
/** Release result state, preserving bindings. Return the previous execution
 * error (OK after ROW/DONE). */
int mulib_sqlite_reset(mulib_sqlite_stmt_t *);
/** Set all bindings to SQL NULL, before execution or after reset. */
int mulib_sqlite_clear_bindings(mulib_sqlite_stmt_t *);
/** Destroy a statement and release its connection reference; NULL is harmless. */
void mulib_sqlite_finalize(mulib_sqlite_stmt_t *);
/** Execute a NUL-terminated SQL script, discarding query results. Earlier
 * autocommitted statements remain committed if a later statement fails. */
int mulib_sqlite_exec(mulib_sqlite_t *,const char *,mulib_sqlite_error_t *);
/** Borrow the connection's last diagnostic, valid until its next operation. */
const mulib_sqlite_error_t *mulib_sqlite_error(const mulib_sqlite_t *);
/** Rows changed by the most recent successful INSERT/UPDATE/DELETE. */
int64_t mulib_sqlite_changes(const mulib_sqlite_t *);
/** Rowid from the last successful INSERT on this connection. */
int64_t mulib_sqlite_last_insert_rowid(const mulib_sqlite_t *);
/** Nonzero when no explicit transaction is active. */
int mulib_sqlite_autocommit(const mulib_sqlite_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SQLITE) && !defined(MULIB_SQLITE_IMPLEMENTED)
#define MULIB_SQLITE_IMPLEMENTED
#include "mulib-peg.h"
#include "mulib-arena.h"
#include "mulib-strbuf.h"
#include "mulib-rng.h"
#include "mulib-utf8.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <inttypes.h>
#include <locale.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Structural rules have capture tags matching this enum. The grammar really
 * recognizes statements and expressions through parsec, not a token-only gate. */
typedef enum mulib_sqlite_node_kind_ {
  MULIB_SQ_INPUT_,MULIB_SQ_STMT_,MULIB_SQ_SELECT_,MULIB_SQ_DISTINCT_,MULIB_SQ_PROJECTION_,MULIB_SQ_ITEM_,
  MULIB_SQ_STAR_,MULIB_SQ_ALIAS_,MULIB_SQ_FROM_,MULIB_SQ_WHERE_,MULIB_SQ_ORDER_,MULIB_SQ_ORDER_ITEM_,MULIB_SQ_DIRECTION_,MULIB_SQ_LIMIT_,MULIB_SQ_OFFSET_,
  MULIB_SQ_CREATE_,MULIB_SQ_IFNOT_,MULIB_SQ_COLUMN_DEF_,MULIB_SQ_TYPENAME_,MULIB_SQ_PRIMARY_,MULIB_SQ_NOTNULL_,MULIB_SQ_DEFAULT_,MULIB_SQ_UNIQUE_,MULIB_SQ_CHECK_,
  MULIB_SQ_INSERT_,MULIB_SQ_COLUMNS_,MULIB_SQ_VALUES_,MULIB_SQ_TUPLE_,MULIB_SQ_DEFAULT_VALUES_,
  MULIB_SQ_UPDATE_,MULIB_SQ_ASSIGN_,MULIB_SQ_DELETE_,MULIB_SQ_DROP_,MULIB_SQ_IFEXISTS_,
  MULIB_SQ_BEGIN_,MULIB_SQ_COMMIT_,MULIB_SQ_ROLLBACK_,MULIB_SQ_SAVEPOINT_,MULIB_SQ_RELEASE_,MULIB_SQ_PRAGMA_,
  MULIB_SQ_EXPR_,MULIB_SQ_OR_,MULIB_SQ_AND_,MULIB_SQ_NOT_,MULIB_SQ_COMPARE_,MULIB_SQ_CMPOP_,MULIB_SQ_CONCAT_,MULIB_SQ_CATOP_,MULIB_SQ_SUM_,MULIB_SQ_ADDOP_,
  MULIB_SQ_PRODUCT_,MULIB_SQ_MULOP_,MULIB_SQ_UNARY_,MULIB_SQ_SIGN_,MULIB_SQ_ATOM_,MULIB_SQ_GROUP_,MULIB_SQ_CALL_,MULIB_SQ_ARGS_,MULIB_SQ_COLUMN_,
  MULIB_SQ_PARAMETER_,MULIB_SQ_BLOB_,MULIB_SQ_STRING_,MULIB_SQ_NUMBER_,MULIB_SQ_NIL_,MULIB_SQ_BOOL_,MULIB_SQ_TABLE_,MULIB_SQ_ID_,MULIB_SQ_NODE_COUNT_
} mulib_sqlite_node_kind_;
static const char mulib_sqlite_grammar_[]=
  "input <- WS (';' WS)* (statement (';' WS / !.) / !.);"
  "statement <- select / create / insert / update / delete / drop / begin / commit / rollback / savepoint / release / pragma;"
  "select <- SELECT distinct? projection from? where? order? limit?;"
  "distinct <- DISTINCT; projection <- item (',' WS item)*; item <- star / expr alias?; star <- '*' WS; alias <- AS id;"
  "from <- FROM table; where <- WHERE expr; order <- ORDER BY order_item (',' WS order_item)*; order_item <- expr direction?; direction <- ASC / DESC;"
  "limit <- LIMIT expr offset?; offset <- OFFSET expr;"
  "create <- CREATE TABLE ifnot? table '(' WS column_def (',' WS column_def)* ')' WS; ifnot <- IF NOT EXISTS;"
  "column_def <- id typename? (primary / notnull / default / unique / check)*;"
  "typename <- id+ ('(' WS number (',' WS number)? ')' WS)?;"
  "primary <- PRIMARY KEY (ASC / DESC)? AUTOINCREMENT?; notnull <- NOT NULL; default <- DEFAULT (group / unary); unique <- UNIQUE; check <- CHECK '(' WS expr ')' WS;"
  "insert <- INSERT INTO table columns? (values / default_values); columns <- '(' WS id (',' WS id)* ')' WS;"
  "values <- VALUES tuple (',' WS tuple)*; tuple <- '(' WS expr (',' WS expr)* ')' WS; default_values <- DEFAULT VALUES;"
  "update <- UPDATE table SET assign (',' WS assign)* where?; assign <- id '=' WS expr;"
  "delete <- DELETE FROM table where?; drop <- DROP TABLE ifexists? table; ifexists <- IF EXISTS;"
  "begin <- BEGIN (DEFERRED / IMMEDIATE / EXCLUSIVE)? TRANSACTION?; commit <- (COMMIT / END) TRANSACTION?;"
  "rollback <- ROLLBACK TRANSACTION? (TO SAVEPOINT? id)?; savepoint <- SAVEPOINT id; release <- RELEASE SAVEPOINT? id;"
  "pragma <- PRAGMA table ('=' WS expr / '(' WS expr ')' WS)?;"
  "expr <- or; or <- and (OR and)*; and <- not (AND not)*; not <- NOT not / compare;"
  "compare <- concat (cmpop concat)?; cmpop <- ('!=' / '<>' / '<=' / '>=' / '==' / '=' / '<' / '>') WS / IS NOT? / LIKE;"
  "concat <- sum (catop sum)*; catop <- '||' WS; sum <- product (addop product)*; addop <- [+-] WS;"
  "product <- unary (mulop unary)*; mulop <- [*/%] WS; unary <- sign unary / atom; sign <- [+-] WS;"
  "atom <- group / blob / string / number / nil / bool / parameter / call / column;"
  "group <- '(' WS expr ')' WS; call <- id '(' WS (star / args)? ')' WS; args <- expr (',' WS expr)*; column <- id ('.' WS id)?;"
  "parameter <- ('?' [0-9]* / [:@$] [A-Za-z_] [A-Za-z_0-9]*) WS;"
  "blob <- [xX] '\\'' ([0-9A-Fa-f] [0-9A-Fa-f])* '\\'' WS;"
  "string <- '\\'' ('\\'\\'' / [^\\'\\x00])* '\\'' WS;"
  "number <- ('0' [xX] [0-9A-Fa-f]+ / ([0-9]+ ('.' [0-9]*)? / '.' [0-9]+) ([eE] [+-]? [0-9]+)?) !MORE WS;"
  "nil <- NULL; bool <- TRUE / FALSE; table <- id ('.' WS id)?;"
  "id <- ('\"' ('\"\"' / [^\"\\x00])* '\"' / '`' ('``' / [^`\\x00])* '`' / '[' [^\\x5d\\x00]* ']' / !keyword [A-Za-z_\\x80-\\xff] MORE*) WS;"
  "MORE <- [A-Za-z_0-9$\\x80-\\xff]; WS <- ([ \\t\\r\\n\\f] / '--' [^\\r\\n]* / '/*' (!'*/' .)* ('*/' / !.))*;";
static const char *const mulib_sqlite_keywords_[]={
  "SELECT","DISTINCT","AS","FROM","WHERE","ORDER","BY","ASC","DESC","LIMIT","OFFSET","CREATE","TABLE","IF","NOT","EXISTS",
  "PRIMARY","KEY","AUTOINCREMENT","NULL","DEFAULT","UNIQUE","CHECK","INSERT","INTO","VALUES","UPDATE","SET","DELETE","DROP",
  "BEGIN","DEFERRED","IMMEDIATE","EXCLUSIVE","TRANSACTION","COMMIT","END","ROLLBACK","TO","SAVEPOINT","RELEASE","PRAGMA","OR","AND","IS","LIKE","TRUE","FALSE",
  "COLLATE","REFERENCES","FOREIGN","CONSTRAINT","GENERATED","ALWAYS","WITHOUT","STRICT","ON","CONFLICT"
};
typedef struct mulib_sqlite_node_ mulib_sqlite_node_;
struct mulib_sqlite_node_ {
  mulib_sqlite_node_kind_ kind; size_t offset,length,parameter;
  char *text; mulib_sqlite_value_t value;
  mulib_sqlite_node_ *child,*next;
};
typedef struct mulib_sqlite_row_ { int64_t rowid; mulib_sqlite_value_t *values; } mulib_sqlite_row_;
typedef struct mulib_sqlite_col_ {
  char *name,*type,*default_sql; int affinity,notnull,primary;
  mulib_sqlite_value_t def;
} mulib_sqlite_col_;
typedef struct mulib_sqlite_table_ {
  char *name,*sql; mulib_sqlite_col_ *cols; size_t ncols,nrows,capacity; int primary;
  mulib_sqlite_row_ *rows;
} mulib_sqlite_table_;
typedef struct mulib_sqlite_save_ {
  char *name; mulib_buffer_t image; int outer,dirty;
  struct mulib_sqlite_save_ *next;
} mulib_sqlite_save_;
struct mulib_sqlite {
  mulib_sqlite_options_t options; mulib_sqlite_pager_t *pager; mulib_peg_t *grammar;
  mulib_sqlite_error_t error; mulib_sqlite_table_ *tables; size_t ntables,statements,steps;
  uint32_t page_size,change,schema,user_version,application_id;
  int blocked,transaction,readonly,poisoned,transaction_dirty; int64_t changes,lastrowid;
  mulib_buffer_t transaction_image; mulib_sqlite_save_ *saves;
  mulib_rng_t rowid_rng;
};
struct mulib_sqlite_stmt {
  mulib_sqlite_t *db; mulib_arena_t arena; const char *source;
  mulib_sqlite_node_ *root; mulib_sqlite_value_t *bindings;
  const char **parameter_names; size_t nparams;
  char **names; mulib_sqlite_value_t *result; size_t ncols,nrows,cursor,result_capacity;
  int state,executed;
};
static int mulib_sqlite_fail_(mulib_sqlite_t *d,int code,size_t off,const char *message) {
  if (d) { d->error.code=code; d->error.offset=off; snprintf(d->error.message,sizeof d->error.message,"%s",message?message:"database error"); }
  return code;
}
static void mulib_sqlite_error_clear_(mulib_sqlite_t *d) { memset(&d->error,0,sizeof d->error); d->error.offset=SIZE_MAX; }
static int mulib_sqlite_case_(const char *a,const char *b) {
  unsigned char x,y;
  do { x=(unsigned char)*a++; y=(unsigned char)*b++; if (x>='a' && x<='z') x-=32; if (y>='a' && y<='z') y-=32; if (x!=y) return x<y?-1:1; } while (x);
  return 0;
}
static char *mulib_sqlite_copy_(const char *s,size_t n) {
  char *p; if (n==SIZE_MAX) return NULL; p=(char *)malloc(n+1); if (p) { if (n) memcpy(p,s,n); p[n]=0; } return p;
}
static void *mulib_sqlite_ast_alloc_(mulib_sqlite_stmt_t *s,size_t n) {
  void *p=mulib_arena_alloc(&s->arena,n?n:1); if (p) memset(p,0,n); return p;
}
static mulib_sqlite_node_ *mulib_sqlite_child_(const mulib_sqlite_node_ *n,mulib_sqlite_node_kind_ k) {
  mulib_sqlite_node_ *c; for (c=n?n->child:NULL;c;c=c->next) if (c->kind==k) return c; return NULL;
}
static int mulib_sqlite_hex_(unsigned char c) { return c<='9'?c-'0':(c|32)-'a'+10; }
static int mulib_sqlite_number_(const char *text,size_t n,mulib_sqlite_value_t *v) {
  char *copy,*end; intmax_t x; double real; size_t i; uint64_t u=0;
  copy=mulib_sqlite_copy_(text,n); if (!copy) return MULIB_SQLITE_NOMEM;
  errno=0; x=strtoimax(copy,&end,10);
  if (end!=copy && !*end && !errno) { v->type=MULIB_SQLITE_INTEGER; v->integer=(int64_t)x; free(copy); return 0; }
  if (n>2 && copy[0]=='0' && (copy[1]=='x' || copy[1]=='X')) {
    if (n>18) { free(copy); return MULIB_SQLITE_ERROR; }
    for (i=2;i<n;++i) u=(u<<4)|(unsigned)mulib_sqlite_hex_((unsigned char)copy[i]);
    v->type=MULIB_SQLITE_INTEGER; v->integer=u<=INT64_MAX?(int64_t)u:-1-(int64_t)(UINT64_MAX-u); free(copy); return 0;
  }
  /* strtod uses the process locale: substitute its decimal separator without
   * changing global locale state. SQL itself always spells decimal points '.'. */
  { const char *decimal=localeconv()->decimal_point; char *dot=strchr(copy,'.');
    if (dot && decimal && strcmp(decimal,".")) {
      mulib_strbuf_t b={0}; size_t at=(size_t)(dot-copy);
      if (!mulib_strbuf_append_n(&b,copy,at) || !mulib_strbuf_append(&b,decimal) || !mulib_strbuf_append(&b,dot+1)) { free(copy); mulib_strbuf_free(&b); return MULIB_SQLITE_NOMEM; }
      free(copy); copy=b.data;
    }
  }
  real=strtod(copy,&end);
  if (end==copy || *end) { free(copy); return MULIB_SQLITE_ERROR; }
  v->type=MULIB_SQLITE_REAL; v->real=real; free(copy); return 0;
}
static int mulib_sqlite_build_grammar_(mulib_sqlite_t *d) {
  mulib_strbuf_t b={0}; size_t i,j; mulib_peg_error_t error; int rc=MULIB_SQLITE_NOMEM;
  if (!mulib_strbuf_append(&b,mulib_sqlite_grammar_) || !mulib_strbuf_append(&b,"keyword <- ")) goto done;
  for (i=0;i<sizeof mulib_sqlite_keywords_/sizeof *mulib_sqlite_keywords_;++i) {
    if ((i && !mulib_strbuf_append(&b," / ")) || !mulib_strbuf_append(&b,mulib_sqlite_keywords_[i])) goto done;
  }
  if (!mulib_strbuf_append(&b,";")) goto done;
  for (i=0;i<sizeof mulib_sqlite_keywords_/sizeof *mulib_sqlite_keywords_;++i) {
    if (!mulib_strbuf_append(&b,mulib_sqlite_keywords_[i]) || !mulib_strbuf_append(&b," <- ")) goto done;
    for (j=0;mulib_sqlite_keywords_[i][j];++j) {
      char pair[5]={'[',mulib_sqlite_keywords_[i][j],(char)(mulib_sqlite_keywords_[i][j]+32),']',0};
      if (!mulib_strbuf_append(&b,pair)) goto done;
    }
    if (!mulib_strbuf_append(&b," !MORE WS;")) goto done;
  }
  d->grammar=mulib_peg_compile(b.data,b.length,&error);
  if (d->grammar) rc=0;
  else mulib_sqlite_fail_(d,rc,error.offset,error.message);
done: mulib_strbuf_free(&b); return rc;
}
static int mulib_sqlite_ast_text_(mulib_sqlite_stmt_t *s,mulib_sqlite_node_ *n) {
  const char *p=s->source+n->offset; size_t i=0,used=0,length=n->length,j; char quote=0,*text;
  if (n->kind==MULIB_SQ_ID_) {
    quote=(*p=='"' || *p=='`' || *p=='[')?*p:0;
    if (quote) { i=1; while (i<length) { char c=p[i++]; if (c==(quote=='['?']':quote)) { if (quote!='[' && i<length && p[i]==c) ++i; else break; } ++used; } }
    else { while (used<length && ((unsigned char)p[used]>=128 || (p[used]>='a' && p[used]<='z') || (p[used]>='A' && p[used]<='Z') || (p[used]>='0' && p[used]<='9') || p[used]=='_' || p[used]=='$')) ++used; }
    text=(char *)mulib_sqlite_ast_alloc_(s,used+1); if (!text) return MULIB_SQLITE_NOMEM;
    if (!quote) memcpy(text,p,used);
    else for (i=1,j=0;j<used;++j) { text[j]=p[i++]; if (quote!='[' && text[j]==quote && p[i]==quote) ++i; }
    n->text=text; return 0;
  }
  if (n->kind==MULIB_SQ_STRING_) {
    for (i=1;i<length;++i) { if (p[i]=='\'') { if (i+1<length && p[i+1]=='\'') ++i; else break; } ++used; }
    text=(char *)mulib_sqlite_ast_alloc_(s,used+1); if (!text) return MULIB_SQLITE_NOMEM;
    for (i=1,j=0;j<used;++j) { text[j]=p[i++]; if (text[j]=='\'' && p[i]=='\'') ++i; }
    n->value.type=MULIB_SQLITE_TEXT; n->value.bytes=(unsigned char *)text; n->value.length=used; return 0;
  }
  if (n->kind==MULIB_SQ_BLOB_) {
    for (i=2;i<length && p[i]!='\'';++i) {}
    used=(i-2)/2; text=(char *)mulib_sqlite_ast_alloc_(s,used+1); if (!text) return MULIB_SQLITE_NOMEM;
    for (j=0;j<used;++j) text[j]=(char)(mulib_sqlite_hex_((unsigned char)p[2+2*j])*16+mulib_sqlite_hex_((unsigned char)p[3+2*j]));
    n->value.type=MULIB_SQLITE_BLOB; n->value.bytes=(unsigned char *)text; n->value.length=used; return 0;
  }
  if (n->kind==MULIB_SQ_NUMBER_) {
    if (length>2 && p[0]=='0' && (p[1]=='x' || p[1]=='X')) {
      used=2; while (used<length && ((p[used]>='0' && p[used]<='9') || (p[used]>='a' && p[used]<='f') || (p[used]>='A' && p[used]<='F'))) ++used;
    } else {
      while (used<length && p[used]>='0' && p[used]<='9') ++used;
      if (used<length && p[used]=='.') { ++used; while (used<length && p[used]>='0' && p[used]<='9') ++used; }
      if (used<length && (p[used]=='e' || p[used]=='E')) {
        ++used; if (used<length && (p[used]=='+' || p[used]=='-')) ++used;
        while (used<length && p[used]>='0' && p[used]<='9') ++used;
      }
    }
    n->text=(char *)mulib_sqlite_ast_alloc_(s,used+1); if (!n->text) return MULIB_SQLITE_NOMEM;
    memcpy(n->text,p,used);
    return mulib_sqlite_number_(p,used,&n->value);
  }
  if (n->kind==MULIB_SQ_BOOL_) { n->value.type=MULIB_SQLITE_INTEGER; n->value.integer=(*p=='t' || *p=='T'); }
  return 0;
}
int mulib_sqlite_prepare(mulib_sqlite_t *d,const char *sql,size_t length,mulib_sqlite_stmt_t **out,size_t *tail,mulib_sqlite_error_t *error) {
  mulib_sqlite_stmt_t *s=NULL; mulib_parsec_result_t result; mulib_sqlite_node_ **stack=NULL,*n;
  size_t *indices=NULL,used=0,i,j,base,count; int rc,status;
  if (error) { memset(error,0,sizeof *error); error->offset=SIZE_MAX; }
  if (!d || !out || (!sql && length)) return MULIB_SQLITE_MISUSE;
  mulib_sqlite_error_clear_(d);
  if (length>d->options.max_sql_bytes || length==SIZE_MAX) { rc=MULIB_SQLITE_TOOBIG; goto fail_early; }
  if (length && memchr(sql,0,length)) { rc=MULIB_SQLITE_ERROR; goto fail_early; }
  status=mulib_peg_run(d->grammar,NULL,sql,length,0,&d->options.parser,&result);
  if (status!=1) {
    rc=status<0?(result.error.code==MULIB_PARSEC_MEMORY?MULIB_SQLITE_NOMEM:MULIB_SQLITE_TOOBIG):MULIB_SQLITE_ERROR;
    mulib_sqlite_fail_(d,rc,result.error.offset,"SQL does not match the supported SQLite grammar");
    mulib_parsec_result_free(&result); if (error) *error=d->error; return rc;
  }
  if (tail) *tail=result.consumed;
  if (result.count>SIZE_MAX/sizeof *stack || result.count>SIZE_MAX/sizeof *indices) { rc=MULIB_SQLITE_TOOBIG; goto fail; }
  s=(mulib_sqlite_stmt_t *)calloc(1,sizeof *s); if (!s) { rc=MULIB_SQLITE_NOMEM; goto fail; }
  s->db=d; ++d->statements; mulib_arena_init(&s->arena,4096);
  { char *copy=(char *)mulib_sqlite_ast_alloc_(s,length+1); if (!copy) { rc=MULIB_SQLITE_NOMEM; goto fail; } if (length) memcpy(copy,sql,length); s->source=copy; }
  stack=(mulib_sqlite_node_ **)malloc((result.count?result.count:1)*sizeof *stack);
  indices=(size_t *)malloc((result.count?result.count:1)*sizeof *indices);
  if (!stack || !indices) { rc=MULIB_SQLITE_NOMEM; goto fail; }
  for (i=0;i<result.count;++i) {
    const mulib_parsec_capture_t *cap=&result.captures[i];
    if (cap->tag>=MULIB_SQ_NODE_COUNT_) continue;
    n=(mulib_sqlite_node_ *)mulib_sqlite_ast_alloc_(s,sizeof *n); if (!n) { rc=MULIB_SQLITE_NOMEM; goto fail; }
    n->kind=(mulib_sqlite_node_kind_)cap->tag; n->offset=cap->offset; n->length=cap->length;
    base=used; while (base && indices[base-1]>=cap->first) --base;
    if (base<used) n->child=stack[base];
    for (j=base;j+1<used;++j) stack[j]->next=stack[j+1];
    used=base; stack[used]=n; indices[used++]=i;
    rc=mulib_sqlite_ast_text_(s,n); if (rc) goto fail;
  }
  s->root=used?stack[0]:NULL;
  s->root=mulib_sqlite_child_(s->root,MULIB_SQ_STMT_);
  if (s->root) s->root=s->root->child;
  /* Parameters receive slots in SQL source order, not capture postorder. */
  count=0;
  s->parameter_names=(const char **)mulib_sqlite_ast_alloc_(s,(d->options.max_columns+1)*sizeof *s->parameter_names);
  if (!s->parameter_names) { rc=MULIB_SQLITE_NOMEM; goto fail; }
  /* Walk the allocated AST via an explicit stack, preserving source order. */
  used=0; if (s->root) stack[used++]=s->root;
  while (used) {
    n=stack[--used];
    if (n->next) stack[used++]=n->next;
    if (n->child) stack[used++]=n->child;
    if (n->kind!=MULIB_SQ_PARAMETER_) continue;
    j=1;
    if (s->source[n->offset]=='?') { while (j<n->length && s->source[n->offset+j]>='0' && s->source[n->offset+j]<='9') ++j; }
    else while (j<n->length && ((s->source[n->offset+j]>='A' && s->source[n->offset+j]<='Z') ||
        (s->source[n->offset+j]>='a' && s->source[n->offset+j]<='z') ||
        (s->source[n->offset+j]>='0' && s->source[n->offset+j]<='9') || s->source[n->offset+j]=='_')) ++j;
    n->text=(char *)mulib_sqlite_ast_alloc_(s,j+1); if (!n->text) { rc=MULIB_SQLITE_NOMEM; goto fail; }
    memcpy(n->text,s->source+n->offset,j);
    base=0;
    if (n->text[0]=='?' && j>1) { uint64_t number=0; for (i=1;i<j;++i) { if (number>d->options.max_columns/10) { rc=MULIB_SQLITE_RANGE; goto fail; } number=number*10+(unsigned)(n->text[i]-'0'); } base=(size_t)number; if (!base) { rc=MULIB_SQLITE_RANGE; goto fail; } }
    else if (j>1) for (i=1;i<=count;++i) if (s->parameter_names[i] && !strcmp(n->text,s->parameter_names[i])) { base=i; break; }
    if (!base) base=count+1;
    if (base>d->options.max_columns) { rc=MULIB_SQLITE_TOOBIG; goto fail; }
    if (base>count) count=base;
    n->parameter=base; if (j>1 && !s->parameter_names[base]) s->parameter_names[base]=n->text;
  }
  s->nparams=count; s->bindings=(mulib_sqlite_value_t *)calloc(count?count:1,sizeof *s->bindings);
  if (!s->bindings) { rc=MULIB_SQLITE_NOMEM; goto fail; }
  free(stack); free(indices); mulib_parsec_result_free(&result);
  if (!s->root) { mulib_sqlite_finalize(s); *out=NULL; return 0; }
  *out=s; return 0;
fail:
  free(stack); free(indices); mulib_parsec_result_free(&result); mulib_sqlite_finalize(s);
fail_early:
  if (!d->error.code) mulib_sqlite_fail_(d,rc,SIZE_MAX,rc==MULIB_SQLITE_NOMEM?"SQL allocation failed":"invalid SQL or resource limit");
  if (error) *error=d->error;
  return rc;
}
/* Remaining layers follow: typed expression evaluation, native storage model,
 * statement execution and public statement lifecycle. */
typedef struct mulib_sqlite_eval_ctx_ {
  mulib_sqlite_stmt_t *stmt; mulib_sqlite_table_ *table; mulib_sqlite_row_ *row;
  mulib_sqlite_row_ **group; size_t group_count,depth; int aggregate;
} mulib_sqlite_eval_ctx_;
static int mulib_sqlite_eval_(mulib_sqlite_eval_ctx_ *,const mulib_sqlite_node_ *,mulib_sqlite_value_t *);
static int mulib_sqlite_tick_(mulib_sqlite_t *d) {
  if (d->steps++>=d->options.max_steps) return mulib_sqlite_fail_(d,MULIB_SQLITE_TOOBIG,SIZE_MAX,"SQL work limit");
  return 0;
}
static int mulib_sqlite_space_byte_(unsigned char c) { return c==' ' || (c>='\t' && c<='\r'); }
static size_t mulib_sqlite_skip_space_(const char *p,size_t n,size_t i) {
  for (;;) {
    while (i<n && mulib_sqlite_space_byte_((unsigned char)p[i])) ++i;
    if (i+1<n && p[i]=='-' && p[i+1]=='-') { i+=2; while (i<n && p[i]!='\r' && p[i]!='\n') ++i; }
    else if (i+1<n && p[i]=='/' && p[i+1]=='*') {
      i+=2; while (i+1<n && (p[i]!='*' || p[i+1]!='/')) ++i;
      i=i+1<n?i+2:n;
    } else return i;
  }
}
static size_t mulib_sqlite_decimal_prefix_(const unsigned char *p,size_t n) {
  size_t i=0,digits=0,exponent;
  if (i<n && (p[i]=='+' || p[i]=='-')) ++i;
  while (i<n && p[i]>='0' && p[i]<='9') { ++i; ++digits; }
  if (i<n && p[i]=='.') { ++i; while (i<n && p[i]>='0' && p[i]<='9') { ++i; ++digits; } }
  if (!digits) return 0;
  if (i<n && (p[i]=='e' || p[i]=='E')) {
    exponent=i++; if (i<n && (p[i]=='+' || p[i]=='-')) ++i;
    digits=0; while (i<n && p[i]>='0' && p[i]<='9') { ++i; ++digits; }
    if (!digits) i=exponent;
  }
  return i;
}
static int mulib_sqlite_numeric_(mulib_sqlite_value_t *v) {
  mulib_sqlite_value_t n={0}; int rc; size_t start=0,end=v->length;
  if (v->type==MULIB_SQLITE_INTEGER || v->type==MULIB_SQLITE_REAL) return 0;
  if (v->type==MULIB_SQLITE_NULL) return 0;
  if (v->type!=MULIB_SQLITE_TEXT || memchr(v->bytes,0,v->length)) return 0;
  while (start<end && mulib_sqlite_space_byte_(v->bytes[start])) ++start;
  while (end>start && mulib_sqlite_space_byte_(v->bytes[end-1])) --end;
  if (start==end || mulib_sqlite_decimal_prefix_(v->bytes+start,end-start)!=end-start) return 0;
  rc=mulib_sqlite_number_((const char *)v->bytes+start,end-start,&n);
  if (rc==MULIB_SQLITE_NOMEM) return rc;
  if (!rc) { mulib_sqlite_value_clear(v); *v=n; }
  return 0;
}
static double mulib_sqlite_real_(const mulib_sqlite_value_t *v) {
  size_t i=0;
  if (v->type==MULIB_SQLITE_INTEGER) return (double)v->integer;
  if (v->type==MULIB_SQLITE_REAL) return v->real;
  if (v->type==MULIB_SQLITE_TEXT || v->type==MULIB_SQLITE_BLOB) {
    while (i<v->length && mulib_sqlite_space_byte_(v->bytes[i])) ++i;
    if (!mulib_sqlite_decimal_prefix_(v->bytes+i,v->length-i)) return 0;
    return strtod((const char *)v->bytes+i,NULL);
  }
  return 0;
}
static int64_t mulib_sqlite_integer_(const mulib_sqlite_value_t *v) {
  double d;
  if (v->type==MULIB_SQLITE_INTEGER) return v->integer;
  d=mulib_sqlite_real_(v); if (d>=9223372036854775808.0) return INT64_MAX;
  if (d<=-9223372036854775808.0) return INT64_MIN;
  return isnan(d)?0:(int64_t)d;
}
static int mulib_sqlite_truth_(const mulib_sqlite_value_t *v) {
  return v->type==MULIB_SQLITE_NULL?-1:mulib_sqlite_real_(v)!=0;
}
static int mulib_sqlite_to_text_(mulib_sqlite_value_t *v) {
  char text[96]; int n;
  if (v->type==MULIB_SQLITE_TEXT || v->type==MULIB_SQLITE_NULL || v->type==MULIB_SQLITE_BLOB) return 0;
  if (v->type==MULIB_SQLITE_INTEGER) n=snprintf(text,sizeof text,"%" PRId64,v->integer);
  else { n=snprintf(text,sizeof text,"%.17g",v->real); if (!strchr(text,'.') && !strchr(text,'e') && !strchr(text,'E') && isfinite(v->real)) { text[n++]='.'; text[n++]='0'; text[n]=0; } }
  return mulib_sqlite_value_bytes(v,MULIB_SQLITE_TEXT,text,(size_t)n);
}
static int mulib_sqlite_compare_values_(const mulib_sqlite_value_t *a,const mulib_sqlite_value_t *b) {
  int ra=a->type==MULIB_SQLITE_NULL?0:a->type<=MULIB_SQLITE_REAL?1:a->type==MULIB_SQLITE_TEXT?2:3;
  int rb=b->type==MULIB_SQLITE_NULL?0:b->type<=MULIB_SQLITE_REAL?1:b->type==MULIB_SQLITE_TEXT?2:3;
  int cmp; double x,y; size_t n;
  if (ra!=rb) return ra<rb?-1:1;
  if (!ra) return 0;
  if (ra==1) {
    if (a->type==MULIB_SQLITE_INTEGER && b->type==MULIB_SQLITE_INTEGER) return (a->integer>b->integer)-(a->integer<b->integer);
    if (a->type==MULIB_SQLITE_REAL && b->type==MULIB_SQLITE_INTEGER) return -mulib_sqlite_compare_values_(b,a);
    if (a->type==MULIB_SQLITE_INTEGER) {
      y=b->real; if (y>=9223372036854775808.0) return -1; if (y< -9223372036854775808.0) return 1;
      if (a->integer!=(int64_t)y) return a->integer<(int64_t)y?-1:1;
      x=(double)a->integer; return (x>y)-(x<y);
    }
    x=a->real; y=b->real; return (x>y)-(x<y);
  }
  n=a->length<b->length?a->length:b->length;
  cmp=n?memcmp(a->bytes,b->bytes,n):0;
  return cmp?cmp:(a->length>b->length)-(a->length<b->length);
}
static int mulib_sqlite_column_index_(mulib_sqlite_table_ *t,const char *name) {
  size_t i;
  if (!t) return -2;
  for (i=0;i<t->ncols;++i) if (!mulib_sqlite_case_(t->cols[i].name,name)) return (int)i;
  if (!mulib_sqlite_case_(name,"rowid") || !mulib_sqlite_case_(name,"_rowid_") || !mulib_sqlite_case_(name,"oid")) return -1;
  return -2;
}
static const mulib_sqlite_node_ *mulib_sqlite_unwrap_(const mulib_sqlite_node_ *n) {
  while (n && n->child && !n->child->next && n->kind!=MULIB_SQ_COLUMN_ && n->kind!=MULIB_SQ_CALL_ && n->kind!=MULIB_SQ_PARAMETER_ &&
      (n->kind!=MULIB_SQ_NOT_ || n->child->kind==MULIB_SQ_COMPARE_))
    n=n->child;
  return n;
}
static int mulib_sqlite_affinity_(mulib_sqlite_value_t *v,int affinity) {
  int rc=0;
  if (affinity==1) rc=mulib_sqlite_to_text_(v);
  else if (affinity>=2) {
    rc=mulib_sqlite_numeric_(v);
    if (!rc && v->type==MULIB_SQLITE_REAL && affinity!=3 && isfinite(v->real) && v->real>=-9223372036854775808.0 && v->real<9223372036854775808.0 && (double)(int64_t)v->real==v->real) {
      v->integer=(int64_t)v->real; v->type=MULIB_SQLITE_INTEGER;
    } else if (!rc && affinity==3 && v->type==MULIB_SQLITE_INTEGER) { v->real=(double)v->integer; v->type=MULIB_SQLITE_REAL; }
  }
  return rc;
}
static int mulib_sqlite_expr_affinity_(mulib_sqlite_eval_ctx_ *e,const mulib_sqlite_node_ *n) {
  int i; n=mulib_sqlite_unwrap_(n);
  if (!n || n->kind!=MULIB_SQ_COLUMN_) return 0;
  n=n->child; if (n->next) n=n->next;
  i=mulib_sqlite_column_index_(e->table,n->text);
  return i>=0?e->table->cols[i].affinity:i==-1?2:0;
}
static int mulib_sqlite_like_(mulib_sqlite_t *d,const unsigned char *text,size_t n,const unsigned char *pattern,size_t m,int *matched) {
  size_t i=0,j=0,star=SIZE_MAX,back=0,take=0,pattern_take=0; uint32_t a,b;
  const unsigned char *zero=(const unsigned char *)memchr(text,0,n);
  if (zero) n=(size_t)(zero-text);
  zero=(const unsigned char *)memchr(pattern,0,m); if (zero) m=(size_t)(zero-pattern);
  if (!mulib_utf8_validate(text,n) || !mulib_utf8_validate(pattern,m)) return MULIB_SQLITE_UNSUPPORTED;
  while (i<n) {
    if (mulib_sqlite_tick_(d)) return d->error.code;
    b=0; pattern_take=0;
    if (j<m && !mulib_utf8_decode(pattern+j,m-j,&b,&pattern_take)) return MULIB_SQLITE_UNSUPPORTED;
    if (b=='%') { j+=pattern_take; star=j; back=i; continue; }
    if (!mulib_utf8_decode(text+i,n-i,&a,&take)) return MULIB_SQLITE_UNSUPPORTED;
    if (a>='A' && a<='Z') a+=32;
    if (b>='A' && b<='Z') b+=32;
    if (j<m && (b=='_' || a==b)) { i+=take; j+=pattern_take; continue; }
    if (star!=SIZE_MAX) {
      if (!mulib_utf8_decode(text+back,n-back,&a,&take)) return MULIB_SQLITE_UNSUPPORTED;
      back+=take; j=star; i=back; continue;
    }
    *matched=0; return 0;
  }
  while (j<m && pattern[j]=='%') ++j;
  *matched=j==m; return 0;
}
static int mulib_sqlite_arithmetic_(int op,mulib_sqlite_value_t *a,mulib_sqlite_value_t *b,mulib_sqlite_value_t *out) {
  int64_t x,y,z=0; double real; int overflow=0,rc;
  if (a->type==MULIB_SQLITE_NULL || b->type==MULIB_SQLITE_NULL) return 0;
  {
    mulib_sqlite_value_t *args[2]={a,b}; size_t i,start,length;
    for (i=0;i<2;++i) if (args[i]->type==MULIB_SQLITE_TEXT || args[i]->type==MULIB_SQLITE_BLOB) {
      mulib_sqlite_value_t number={0}; start=0;
      while (start<args[i]->length && mulib_sqlite_space_byte_(args[i]->bytes[start])) ++start;
      length=mulib_sqlite_decimal_prefix_(args[i]->bytes+start,args[i]->length-start);
      if (length) { rc=mulib_sqlite_number_((const char *)args[i]->bytes+start,length,&number); if (rc) return rc; }
      else number.type=MULIB_SQLITE_INTEGER;
      mulib_sqlite_value_clear(args[i]); *args[i]=number;
    }
  }
  if (a->type==MULIB_SQLITE_INTEGER && b->type==MULIB_SQLITE_INTEGER) {
    x=a->integer; y=b->integer;
    if (op=='+') { overflow=(y>0 && x>INT64_MAX-y)||(y<0 && x<INT64_MIN-y); if (!overflow) z=x+y; }
    else if (op=='-') { overflow=(y<0 && x>INT64_MAX+y)||(y>0 && x<INT64_MIN+y); if (!overflow) z=x-y; }
    else if (op=='*') {
      if (x>0) overflow=y>0?x>INT64_MAX/y:y<0 && y<INT64_MIN/x;
      else if (x<0) overflow=y>0?x<INT64_MIN/y:y<0 && x<INT64_MAX/y;
      if (!overflow) z=x*y;
    } else if (op=='/') { if (!y) return 0; overflow=x==INT64_MIN && y==-1; if (!overflow) z=x/y; }
    else { if (!y) return 0; z=(x==INT64_MIN && y==-1)?0:x%y; }
    if (!overflow) { out->type=MULIB_SQLITE_INTEGER; out->integer=z; return 0; }
  }
  if ((op=='/' || op=='%') && mulib_sqlite_real_(b)==0) return 0;
  real=op=='+'?mulib_sqlite_real_(a)+mulib_sqlite_real_(b):op=='-'?mulib_sqlite_real_(a)-mulib_sqlite_real_(b):
       op=='*'?mulib_sqlite_real_(a)*mulib_sqlite_real_(b):op=='/'?mulib_sqlite_real_(a)/mulib_sqlite_real_(b):0;
  if (op=='%') { x=mulib_sqlite_integer_(a); y=mulib_sqlite_integer_(b); if (!y) return 0; real=(double)((x==INT64_MIN && y==-1)?0:x%y); }
  if (!isnan(real)) { out->type=MULIB_SQLITE_REAL; out->real=real; } return 0;
}
static int mulib_sqlite_aggregate_name_(const char *name) {
  return !mulib_sqlite_case_(name,"count") || !mulib_sqlite_case_(name,"sum") || !mulib_sqlite_case_(name,"total") ||
    !mulib_sqlite_case_(name,"avg") || !mulib_sqlite_case_(name,"min") || !mulib_sqlite_case_(name,"max");
}
static int mulib_sqlite_has_aggregate_(const mulib_sqlite_node_ *n) {
  const mulib_sqlite_node_ *c;
  if (!n) return 0;
  if (n->kind==MULIB_SQ_CALL_ && mulib_sqlite_aggregate_name_(n->child->text)) return 1;
  for (c=n->child;c;c=c->next) if (mulib_sqlite_has_aggregate_(c)) return 1;
  return 0;
}
static const mulib_sqlite_node_ *mulib_sqlite_extreme_call_(const mulib_sqlite_node_ *n) {
  const mulib_sqlite_node_ *c,*found;
  if (!n) return NULL;
  if (n->kind==MULIB_SQ_CALL_ && (!mulib_sqlite_case_(n->child->text,"min") || !mulib_sqlite_case_(n->child->text,"max"))) return n;
  for (c=n->child;c;c=c->next) { found=mulib_sqlite_extreme_call_(c); if (found) return found; }
  return NULL;
}
static int mulib_sqlite_call_(mulib_sqlite_eval_ctx_ *e,const mulib_sqlite_node_ *n,mulib_sqlite_value_t *out) {
  const char *name=n->child->text; const mulib_sqlite_node_ *args=n->child->next,*arg;
  mulib_sqlite_value_t a={0},b={0},acc={0}; size_t count=0,i,values=0; int rc=0,cmp,is_count,star;
  if (args && args->kind==MULIB_SQ_ARGS_) args=args->child;
  for (arg=args;arg;arg=arg->next) ++count;
  star=args && args->kind==MULIB_SQ_STAR_;
  if (mulib_sqlite_aggregate_name_(name)) {
    if (!e->aggregate || count!=1 || (star && mulib_sqlite_case_(name,"count"))) return MULIB_SQLITE_ERROR;
    is_count=!mulib_sqlite_case_(name,"count");
    for (i=0;i<e->group_count;++i) {
      mulib_sqlite_eval_ctx_ sub=*e; sub.row=e->group[i]; sub.aggregate=0;
      if (!star) { rc=mulib_sqlite_eval_(&sub,args,&a); if (rc) goto done; }
      if (!star && a.type==MULIB_SQLITE_NULL) { mulib_sqlite_value_clear(&a); continue; }
      ++values;
      if (!is_count) {
        if (!mulib_sqlite_case_(name,"min") || !mulib_sqlite_case_(name,"max")) {
          cmp=mulib_sqlite_compare_values_(&a,&acc);
          if (values==1 || (!mulib_sqlite_case_(name,"min")?cmp<0:cmp>0)) { mulib_sqlite_value_clear(&acc); acc=a; memset(&a,0,sizeof a); }
        } else {
          if (values==1) { acc.type=MULIB_SQLITE_INTEGER; acc.integer=0; }
          rc=mulib_sqlite_arithmetic_('+',&acc,&a,&b); if (rc) goto done;
          if (!mulib_sqlite_case_(name,"sum") && acc.type==MULIB_SQLITE_INTEGER && a.type==MULIB_SQLITE_INTEGER && b.type==MULIB_SQLITE_REAL) { rc=MULIB_SQLITE_ERROR; goto done; }
          mulib_sqlite_value_clear(&acc); acc=b; memset(&b,0,sizeof b);
        }
      }
      mulib_sqlite_value_clear(&a);
    }
    if (is_count) { out->type=MULIB_SQLITE_INTEGER; out->integer=(int64_t)values; }
    else if (!mulib_sqlite_case_(name,"total") || (!mulib_sqlite_case_(name,"avg") && values)) {
      out->type=MULIB_SQLITE_REAL; out->real=mulib_sqlite_real_(&acc)/(!mulib_sqlite_case_(name,"avg")?(double)values:1);
    } else { *out=acc; memset(&acc,0,sizeof acc); }
    goto done;
  }
  if (!mulib_sqlite_case_(name,"coalesce") || !mulib_sqlite_case_(name,"ifnull")) {
    if (count<2 || (!mulib_sqlite_case_(name,"ifnull") && count!=2)) return MULIB_SQLITE_ERROR;
    for (arg=args;arg;arg=arg->next) { rc=mulib_sqlite_eval_(e,arg,&a); if (rc) goto done; if (a.type!=MULIB_SQLITE_NULL) { *out=a; memset(&a,0,sizeof a); goto done; } }
    goto done;
  }
  if (count!=1 || star) return MULIB_SQLITE_UNSUPPORTED;
  rc=mulib_sqlite_eval_(e,args,&a); if (rc) goto done;
  if (!mulib_sqlite_case_(name,"typeof")) {
    static const char *const types[]={"null","integer","real","text","blob"};
    rc=mulib_sqlite_value_bytes(out,MULIB_SQLITE_TEXT,types[a.type],strlen(types[a.type])); goto done;
  }
  if (a.type==MULIB_SQLITE_NULL) goto done;
  if (!mulib_sqlite_case_(name,"length")) {
    rc=mulib_sqlite_to_text_(&a); if (rc) goto done;
    values=0;
    if (a.type==MULIB_SQLITE_BLOB) values=a.length;
    else for (i=0;i<a.length && a.bytes[i];++i) if ((a.bytes[i]&192)!=128) ++values;
    out->type=MULIB_SQLITE_INTEGER; out->integer=(int64_t)values;
  } else if (!mulib_sqlite_case_(name,"abs")) {
    int original=a.type;
    rc=mulib_sqlite_numeric_(&a); if (rc) goto done;
    if (original==MULIB_SQLITE_INTEGER) { if (a.integer==INT64_MIN) { rc=MULIB_SQLITE_ERROR; goto done; } out->type=MULIB_SQLITE_INTEGER; out->integer=a.integer<0?-a.integer:a.integer; }
    else { out->type=MULIB_SQLITE_REAL; out->real=fabs(mulib_sqlite_real_(&a)); }
  } else if (!mulib_sqlite_case_(name,"upper") || !mulib_sqlite_case_(name,"lower")) {
    rc=mulib_sqlite_to_text_(&a); if (rc) goto done;
    for (i=0;i<a.length;++i) { if (!mulib_sqlite_case_(name,"upper") && a.bytes[i]>='a' && a.bytes[i]<='z') a.bytes[i]-=32; else if (!mulib_sqlite_case_(name,"lower") && a.bytes[i]>='A' && a.bytes[i]<='Z') a.bytes[i]+=32; }
    a.type=MULIB_SQLITE_TEXT; *out=a; memset(&a,0,sizeof a);
  } else rc=MULIB_SQLITE_UNSUPPORTED;
done: mulib_sqlite_value_clear(&a); mulib_sqlite_value_clear(&b); mulib_sqlite_value_clear(&acc); return rc;
}
static int mulib_sqlite_eval_(mulib_sqlite_eval_ctx_ *e,const mulib_sqlite_node_ *n,mulib_sqlite_value_t *out) {
  mulib_sqlite_t *d=e->stmt->db; const mulib_sqlite_node_ *c,*op,*right;
  mulib_sqlite_value_t a={0},b={0},value={0}; int rc=0,i,ta,tb,cmp,affinity;
  const char *spelling; size_t length;
  if (!n || ++e->depth>128) { --e->depth; return MULIB_SQLITE_TOOBIG; }
  rc=mulib_sqlite_tick_(d); if (rc) goto done;
  switch (n->kind) {
    case MULIB_SQ_NIL_: break;
    case MULIB_SQ_NUMBER_: case MULIB_SQ_STRING_: case MULIB_SQ_BLOB_: case MULIB_SQ_BOOL_:
      rc=mulib_sqlite_value_copy(out,&n->value); break;
    case MULIB_SQ_PARAMETER_: rc=mulib_sqlite_value_copy(out,&e->stmt->bindings[n->parameter-1]); break;
    case MULIB_SQ_COLUMN_:
      c=n->child; if (c->next) { if (!e->table || mulib_sqlite_case_(e->table->name,c->text)) { rc=MULIB_SQLITE_ERROR; break; } c=c->next; }
      i=mulib_sqlite_column_index_(e->table,c->text);
      if (i==-2 || (!e->row && !e->aggregate)) rc=MULIB_SQLITE_ERROR;
      else if (!e->row) break;
      else if (i==-1) { out->type=MULIB_SQLITE_INTEGER; out->integer=e->row->rowid; }
      else rc=mulib_sqlite_value_copy(out,&e->row->values[i]);
      break;
    case MULIB_SQ_CALL_: rc=mulib_sqlite_call_(e,n,out); break;
    case MULIB_SQ_NOT_:
      if (n->child->kind==MULIB_SQ_COMPARE_) { rc=mulib_sqlite_eval_(e,n->child,out); break; }
      rc=mulib_sqlite_eval_(e,n->child,&a); if (!rc) { ta=mulib_sqlite_truth_(&a); if (ta>=0) { out->type=MULIB_SQLITE_INTEGER; out->integer=!ta; } }
      break;
    case MULIB_SQ_UNARY_:
      if (n->child->kind!=MULIB_SQ_SIGN_) { rc=mulib_sqlite_eval_(e,n->child,out); break; }
      c=n->child; right=c->next;
      rc=mulib_sqlite_eval_(e,right,&a); if (rc) break;
      if (e->stmt->source[c->offset]=='+') { *out=a; memset(&a,0,sizeof a); break; }
      if (a.type==MULIB_SQLITE_NULL) break;
      rc=mulib_sqlite_numeric_(&a); if (rc) break;
      if (a.type==MULIB_SQLITE_INTEGER && a.integer!=INT64_MIN) { out->type=MULIB_SQLITE_INTEGER; out->integer=-a.integer; }
      else {
        const mulib_sqlite_node_ *literal=mulib_sqlite_unwrap_(right);
        if (literal && literal->kind==MULIB_SQ_NUMBER_ && !strcmp(literal->text,"9223372036854775808")) { out->type=MULIB_SQLITE_INTEGER; out->integer=INT64_MIN; }
        else { out->type=MULIB_SQLITE_REAL; out->real=-mulib_sqlite_real_(&a); }
      }
      break;
    case MULIB_SQ_OR_: case MULIB_SQ_AND_:
      rc=mulib_sqlite_eval_(e,n->child,&a);
      for (c=n->child->next;c && !rc;c=c->next) {
        rc=mulib_sqlite_eval_(e,c,&b); if (rc) break;
        ta=mulib_sqlite_truth_(&a); tb=mulib_sqlite_truth_(&b); mulib_sqlite_value_clear(&a);
        i=n->kind==MULIB_SQ_AND_?(ta==0 || tb==0?0:ta<0 || tb<0?-1:1):(ta==1 || tb==1?1:ta<0 || tb<0?-1:0);
        if (i>=0) { a.type=MULIB_SQLITE_INTEGER; a.integer=i; } mulib_sqlite_value_clear(&b);
      }
      if (!rc) { *out=a; memset(&a,0,sizeof a); } break;
    case MULIB_SQ_COMPARE_: case MULIB_SQ_CONCAT_: case MULIB_SQ_SUM_: case MULIB_SQ_PRODUCT_:
      rc=mulib_sqlite_eval_(e,n->child,&a);
      for (op=n->child->next;op && !rc;op=right->next) {
        right=op->next; rc=mulib_sqlite_eval_(e,right,&b); if (rc) break;
        spelling=e->stmt->source+op->offset;
        if (n->kind==MULIB_SQ_COMPARE_) {
          affinity=mulib_sqlite_expr_affinity_(e,n->child); i=mulib_sqlite_expr_affinity_(e,right);
          if (affinity>=2 && i<2) rc=mulib_sqlite_affinity_(&b,2);
          else if (i>=2 && affinity<2) rc=mulib_sqlite_affinity_(&a,2);
          else if (affinity==1 && !i) rc=mulib_sqlite_affinity_(&b,1);
          else if (i==1 && !affinity) rc=mulib_sqlite_affinity_(&a,1);
          if (rc) break;
          if ((*spelling=='I' || *spelling=='i')) {
            length=mulib_sqlite_skip_space_(spelling,op->length,2);
            i=length<op->length && (spelling[length]=='N' || spelling[length]=='n');
            cmp=mulib_sqlite_compare_values_(&a,&b); value.type=MULIB_SQLITE_INTEGER; value.integer=i?(cmp!=0):(cmp==0);
          } else if (a.type!=MULIB_SQLITE_NULL && b.type!=MULIB_SQLITE_NULL) {
            cmp=mulib_sqlite_compare_values_(&a,&b); value.type=MULIB_SQLITE_INTEGER;
            if (*spelling=='L' || *spelling=='l') {
              if (a.type==MULIB_SQLITE_BLOB || b.type==MULIB_SQLITE_BLOB) { rc=MULIB_SQLITE_UNSUPPORTED; break; }
              rc=mulib_sqlite_to_text_(&a); if (!rc) rc=mulib_sqlite_to_text_(&b);
              if (!rc) rc=mulib_sqlite_like_(d,a.bytes,a.length,b.bytes,b.length,&i);
              value.integer=i;
            } else value.integer=*spelling=='='?cmp==0:*spelling=='!'?cmp!=0:*spelling=='<'?(spelling[1]=='='?cmp<=0:spelling[1]=='>'?cmp!=0:cmp<0):spelling[1]=='='?cmp>=0:cmp>0;
          }
        } else if (n->kind==MULIB_SQ_CONCAT_) {
          if (a.type!=MULIB_SQLITE_NULL && b.type!=MULIB_SQLITE_NULL) {
            mulib_buffer_t bytes={0}; rc=mulib_sqlite_to_text_(&a); if (!rc) rc=mulib_sqlite_to_text_(&b);
            if (!rc && (a.length>d->options.max_bytes || b.length>d->options.max_bytes-a.length)) rc=MULIB_SQLITE_TOOBIG;
            if (!rc && (!mulib_buffer_append(&bytes,a.bytes,a.length) || !mulib_buffer_append(&bytes,b.bytes,b.length))) rc=MULIB_SQLITE_NOMEM;
            if (!rc) rc=mulib_sqlite_value_bytes(&value,MULIB_SQLITE_TEXT,bytes.data,bytes.length);
            mulib_buffer_free(&bytes);
          }
        } else rc=mulib_sqlite_arithmetic_(*spelling,&a,&b,&value);
        mulib_sqlite_value_clear(&a); mulib_sqlite_value_clear(&b); a=value; memset(&value,0,sizeof value);
      }
      if (!rc) { *out=a; memset(&a,0,sizeof a); } break;
    default: rc=n->child?mulib_sqlite_eval_(e,n->child,out):MULIB_SQLITE_ERROR; break;
  }
done: mulib_sqlite_value_clear(&a); mulib_sqlite_value_clear(&b); mulib_sqlite_value_clear(&value); --e->depth; return rc;
}
static void mulib_sqlite_table_free_(mulib_sqlite_table_ *t) {
  size_t i;
  for (i=0;i<t->nrows;++i) mulib_sqlite_format_values_free(t->rows[i].values,t->ncols);
  if (t->cols) for (i=0;i<t->ncols;++i) { free(t->cols[i].name); free(t->cols[i].type); free(t->cols[i].default_sql); mulib_sqlite_value_clear(&t->cols[i].def); }
  free(t->rows); free(t->cols); free(t->name); free(t->sql); memset(t,0,sizeof *t);
}
static void mulib_sqlite_tables_free_(mulib_sqlite_t *d) {
  size_t i; for (i=0;i<d->ntables;++i) mulib_sqlite_table_free_(&d->tables[i]); free(d->tables); d->tables=NULL; d->ntables=0;
}
static int mulib_sqlite_type_affinity_(const char *type) {
  char upper[256]; size_t i,n=strlen(type); if (n>=sizeof upper) n=sizeof upper-1;
  for (i=0;i<n;++i) upper[i]=type[i]>='a' && type[i]<='z'?(char)(type[i]-32):type[i];
  upper[n]=0;
  if (strstr(upper,"INT")) return 2;
  if (strstr(upper,"CHAR") || strstr(upper,"CLOB") || strstr(upper,"TEXT")) return 1;
  if (!n || strstr(upper,"BLOB")) return 0;
  if (strstr(upper,"REAL") || strstr(upper,"FLOA") || strstr(upper,"DOUB")) return 3;
  return 4;
}
static int mulib_sqlite_table_name_(const mulib_sqlite_node_ *n,const char **name) {
  n=n->child;
  if (n->next) { if (mulib_sqlite_case_(n->text,"main")) return MULIB_SQLITE_UNSUPPORTED; n=n->next; }
  *name=n->text; return 0;
}
static mulib_sqlite_table_ *mulib_sqlite_find_table_(mulib_sqlite_t *d,const char *name) {
  size_t i;
  if (!mulib_sqlite_case_(name,"sqlite_master")) name="sqlite_schema";
  for (i=0;i<d->ntables;++i) if (!mulib_sqlite_case_(d->tables[i].name,name)) return &d->tables[i];
  return NULL;
}
static int mulib_sqlite_constant_(const mulib_sqlite_node_ *n) {
  const mulib_sqlite_node_ *c;
  if (n->kind==MULIB_SQ_PARAMETER_ || n->kind==MULIB_SQ_COLUMN_ || n->kind==MULIB_SQ_CALL_) return 0;
  for (c=n->child;c;c=c->next) if (!mulib_sqlite_constant_(c)) return 0;
  return 1;
}
static int mulib_sqlite_column_defs_(mulib_sqlite_stmt_t *s,mulib_sqlite_table_ *t) {
  mulib_sqlite_node_ *n,*c,*type,*primary,*def; size_t i=0,j,count=0; int rc;
  mulib_sqlite_eval_ctx_ eval; memset(&eval,0,sizeof eval); eval.stmt=s;
  for (n=s->root->child;n;n=n->next) if (n->kind==MULIB_SQ_COLUMN_DEF_) ++count;
  if (!count || count>s->db->options.max_columns) return MULIB_SQLITE_TOOBIG;
  t->cols=(mulib_sqlite_col_ *)calloc(count,sizeof *t->cols); if (!t->cols) return MULIB_SQLITE_NOMEM;
  t->ncols=count; t->primary=-1;
  for (n=s->root->child;n;n=n->next) if (n->kind==MULIB_SQ_COLUMN_DEF_) {
    c=n->child; t->cols[i].name=mulib_sqlite_copy_(c->text,strlen(c->text));
    type=mulib_sqlite_child_(n,MULIB_SQ_TYPENAME_);
    if (type) {
      size_t length=type->length;
      while (length && strchr(" \t\r\n\f",s->source[type->offset+length-1])) --length;
      t->cols[i].type=mulib_sqlite_copy_(s->source+type->offset,length);
    } else t->cols[i].type=mulib_sqlite_copy_("",0);
    if (!t->cols[i].name || !t->cols[i].type) return MULIB_SQLITE_NOMEM;
    for (j=0;j<i;++j) if (!mulib_sqlite_case_(t->cols[j].name,t->cols[i].name)) return MULIB_SQLITE_ERROR;
    t->cols[i].affinity=mulib_sqlite_type_affinity_(t->cols[i].type);
    t->cols[i].notnull=mulib_sqlite_child_(n,MULIB_SQ_NOTNULL_)!=NULL;
    if (mulib_sqlite_child_(n,MULIB_SQ_UNIQUE_) || mulib_sqlite_child_(n,MULIB_SQ_CHECK_)) return MULIB_SQLITE_UNSUPPORTED;
    primary=mulib_sqlite_child_(n,MULIB_SQ_PRIMARY_);
    if (primary) {
      char *spelling=mulib_sqlite_copy_(s->source+primary->offset,primary->length);
      if (!spelling) return MULIB_SQLITE_NOMEM;
      for (j=0;spelling[j];++j) if (spelling[j]>='a' && spelling[j]<='z') spelling[j]-=32;
      rc=(strstr(spelling,"DESC") || strstr(spelling,"AUTOINCREMENT") || mulib_sqlite_case_(t->cols[i].type,"INTEGER") || t->primary>=0)?MULIB_SQLITE_UNSUPPORTED:0;
      free(spelling); if (rc) return rc;
      t->primary=(int)i; t->cols[i].primary=1;
    }
    def=mulib_sqlite_child_(n,MULIB_SQ_DEFAULT_);
    if (def) {
      mulib_sqlite_node_ *expression=def->child; size_t length;
      if (!mulib_sqlite_constant_(expression)) return MULIB_SQLITE_UNSUPPORTED;
      rc=mulib_sqlite_eval_(&eval,expression,&t->cols[i].def); if (rc) return rc;
      rc=mulib_sqlite_affinity_(&t->cols[i].def,t->cols[i].affinity); if (rc) return rc;
      if (expression->kind==MULIB_SQ_GROUP_) expression=expression->child;
      length=expression->length; while (length && strchr(" \t\r\n\f",s->source[expression->offset+length-1])) --length;
      t->cols[i].default_sql=mulib_sqlite_copy_(s->source+expression->offset,length); if (!t->cols[i].default_sql) return MULIB_SQLITE_NOMEM;
    }
    ++i;
  }
  return 0;
}
static int mulib_sqlite_append_row_(mulib_sqlite_t *d,mulib_sqlite_table_ *t,int64_t id,mulib_sqlite_value_t *values) {
  mulib_sqlite_row_ *rows; size_t capacity;
  if (t->nrows>=d->options.max_rows) return MULIB_SQLITE_TOOBIG;
  if (t->nrows==t->capacity) {
    capacity=t->capacity?t->capacity*2:16;
    if (capacity<t->capacity || capacity>SIZE_MAX/sizeof *rows) return MULIB_SQLITE_TOOBIG;
    rows=(mulib_sqlite_row_ *)realloc(t->rows,capacity*sizeof *rows); if (!rows) return MULIB_SQLITE_NOMEM;
    t->rows=rows; t->capacity=capacity;
  }
  t->rows[t->nrows].rowid=id; t->rows[t->nrows++].values=values; return 0;
}
typedef struct mulib_sqlite_load_ { mulib_sqlite_t *db; mulib_sqlite_table_ *table; } mulib_sqlite_load_;
static int mulib_sqlite_load_row_(void *user,int64_t id,const void *record,size_t length) {
  mulib_sqlite_load_ *load=(mulib_sqlite_load_ *)user; mulib_sqlite_table_ *t=load->table;
  mulib_sqlite_value_t *values=NULL,*full=NULL; size_t count=0,i; int rc;
  rc=mulib_sqlite_format_decode(record,length,load->db->options.max_columns,&values,&count); if (rc) return rc;
  if (count>t->ncols) { rc=MULIB_SQLITE_CORRUPT; goto fail; }
  full=(mulib_sqlite_value_t *)calloc(t->ncols,sizeof *full); if (!full) { rc=MULIB_SQLITE_NOMEM; goto fail; }
  for (i=0;i<t->ncols;++i) {
    if ((int)i==t->primary) { full[i].type=MULIB_SQLITE_INTEGER; full[i].integer=id; }
    else { rc=mulib_sqlite_value_copy(&full[i],i<count?&values[i]:&t->cols[i].def); if (rc) goto fail; }
    if (t->cols[i].affinity==3 && full[i].type==MULIB_SQLITE_INTEGER) { full[i].type=MULIB_SQLITE_REAL; full[i].real=(double)full[i].integer; }
  }
  rc=mulib_sqlite_append_row_(load->db,t,id,full); if (!rc) full=NULL;
fail: mulib_sqlite_format_values_free(values,count); mulib_sqlite_format_values_free(full,t->ncols); return rc;
}
static int mulib_sqlite_schema_init_(mulib_sqlite_t *d) {
  static const char *const names[]={"type","name","tbl_name","rootpage","sql"};
  mulib_sqlite_table_ *t; size_t i;
  d->tables=(mulib_sqlite_table_ *)calloc(1,sizeof *d->tables); if (!d->tables) return MULIB_SQLITE_NOMEM;
  d->ntables=1; t=d->tables; t->primary=-1; t->ncols=5;
  t->name=mulib_sqlite_copy_("sqlite_schema",13); t->cols=(mulib_sqlite_col_ *)calloc(5,sizeof *t->cols);
  if (!t->name || !t->cols) return MULIB_SQLITE_NOMEM;
  for (i=0;i<5;++i) { t->cols[i].name=mulib_sqlite_copy_(names[i],strlen(names[i])); t->cols[i].type=mulib_sqlite_copy_(i==3?"INTEGER":"TEXT",i==3?7:4); t->cols[i].affinity=i==3?2:1; if (!t->cols[i].name || !t->cols[i].type) return MULIB_SQLITE_NOMEM; }
  return 0;
}
static int mulib_sqlite_load_image_(mulib_sqlite_t *d,const void *image,size_t length) {
  mulib_sqlite_t fresh; mulib_sqlite_format_t f; mulib_sqlite_load_ load;
  mulib_sqlite_table_ *table,*grown; mulib_sqlite_stmt_t *stmt=NULL;
  mulib_sqlite_value_t *v; const char *name; size_t i,tail; int rc;
  memset(&fresh,0,sizeof fresh); fresh.options=d->options; fresh.grammar=d->grammar;
  rc=mulib_sqlite_schema_init_(&fresh); if (rc) goto fail;
  if (!length) { fresh.page_size=4096; goto success; }
  rc=mulib_sqlite_format_open(&f,image,length); if (rc) goto fail;
  if ((f.encoding && f.encoding!=1) || f.data[18]!=1 || f.data[19]!=1) { rc=MULIB_SQLITE_UNSUPPORTED; goto fail; }
  fresh.page_size=f.page_size; fresh.change=mulib_endian_load_be32(f.data+24); fresh.schema=mulib_endian_load_be32(f.data+40);
  fresh.user_version=mulib_endian_load_be32(f.data+60); fresh.application_id=mulib_endian_load_be32(f.data+68);
  fresh.blocked=f.data[20] || mulib_endian_load_be32(f.data+52) || mulib_endian_load_be32(f.data+64) ||
    (f.schema_format!=4 && f.schema_format!=0);
  load.db=&fresh; load.table=fresh.tables;
  rc=mulib_sqlite_format_scan(&f,1,d->options.max_bytes,mulib_sqlite_load_row_,&load); if (rc) goto fail;
  if (!f.schema_format && fresh.tables[0].nrows) { rc=MULIB_SQLITE_CORRUPT; goto fail; }
  for (i=0;i<fresh.tables[0].nrows;++i) {
    v=fresh.tables[0].rows[i].values;
    if (v[0].type!=MULIB_SQLITE_TEXT || v[1].type!=MULIB_SQLITE_TEXT || v[2].type!=MULIB_SQLITE_TEXT || v[3].type!=MULIB_SQLITE_INTEGER ||
        memchr(v[0].bytes,0,v[0].length) || memchr(v[1].bytes,0,v[1].length)) { rc=MULIB_SQLITE_CORRUPT; goto fail; }
    if (strcmp((const char *)v[0].bytes,"table")) { fresh.blocked=1; continue; }
    if (v[4].type!=MULIB_SQLITE_TEXT || memchr(v[4].bytes,0,v[4].length) || v[3].integer<=0 || (uint64_t)v[3].integer>f.page_count) { rc=MULIB_SQLITE_UNSUPPORTED; goto fail; }
    rc=mulib_sqlite_prepare(&fresh,(const char *)v[4].bytes,v[4].length,&stmt,&tail,NULL);
    if (rc || !stmt || stmt->root->kind!=MULIB_SQ_CREATE_ || tail!=v[4].length) { rc=rc==MULIB_SQLITE_NOMEM?rc:MULIB_SQLITE_UNSUPPORTED; goto fail; }
    rc=mulib_sqlite_table_name_(mulib_sqlite_child_(stmt->root,MULIB_SQ_TABLE_),&name); if (rc) goto fail;
    if (mulib_sqlite_case_(name,(const char *)v[1].bytes) || mulib_sqlite_find_table_(&fresh,name)) { rc=MULIB_SQLITE_CORRUPT; goto fail; }
    if (fresh.ntables>=d->options.max_columns || fresh.ntables==SIZE_MAX/sizeof *grown) { rc=MULIB_SQLITE_TOOBIG; goto fail; }
    grown=(mulib_sqlite_table_ *)realloc(fresh.tables,(fresh.ntables+1)*sizeof *grown); if (!grown) { rc=MULIB_SQLITE_NOMEM; goto fail; }
    fresh.tables=grown; table=&grown[fresh.ntables++]; memset(table,0,sizeof *table);
    table->name=mulib_sqlite_copy_(name,strlen(name)); table->sql=mulib_sqlite_copy_((const char *)v[4].bytes,v[4].length);
    if (!table->name || !table->sql) { rc=MULIB_SQLITE_NOMEM; goto fail; }
    rc=mulib_sqlite_column_defs_(stmt,table); if (rc) goto fail;
    mulib_sqlite_finalize(stmt); stmt=NULL;
    load.table=table;
    rc=mulib_sqlite_format_scan(&f,(uint32_t)v[3].integer,d->options.max_bytes,mulib_sqlite_load_row_,&load); if (rc) goto fail;
  }
success:
  mulib_sqlite_tables_free_(d); d->tables=fresh.tables; d->ntables=fresh.ntables;
  d->page_size=fresh.page_size; d->change=fresh.change; d->schema=fresh.schema;
  d->user_version=fresh.user_version; d->application_id=fresh.application_id; d->blocked=fresh.blocked; return 0;
fail: mulib_sqlite_finalize(stmt); mulib_sqlite_tables_free_(&fresh); return rc;
}
static int mulib_sqlite_row_order_(const void *a,const void *b) {
  const mulib_sqlite_row_ *x=(const mulib_sqlite_row_ *)a,*y=(const mulib_sqlite_row_ *)b;
  return (x->rowid>y->rowid)-(x->rowid<y->rowid);
}
static int mulib_sqlite_image_(mulib_sqlite_t *d,mulib_buffer_t *out) {
  mulib_sqlite_format_builder_t b; mulib_sqlite_format_row_t *rows=NULL;
  mulib_buffer_t *records=NULL; mulib_sqlite_value_t values[5];
  size_t i,j,k,count; uint32_t *roots=NULL; int rc;
  if (d->blocked) return MULIB_SQLITE_UNSUPPORTED;
  memset(&b,0,sizeof b); memset(values,0,sizeof values);
  rc=mulib_sqlite_format_builder_init(&b,d->page_size,d->options.max_bytes); if (rc) return rc;
  roots=(uint32_t *)calloc(d->ntables,sizeof *roots); if (!roots) { rc=MULIB_SQLITE_NOMEM; goto done; }
  roots[0]=1;
  for (i=1;i<d->ntables;++i) { roots[i]=mulib_sqlite_format_page(&b); if (!roots[i]) { rc=MULIB_SQLITE_NOMEM; goto done; } }
  for (i=0;i<d->ntables;++i) {
    mulib_sqlite_table_ *t=&d->tables[i]; count=i?t->nrows:d->ntables-1;
    rows=(mulib_sqlite_format_row_t *)calloc(count?count:1,sizeof *rows); records=(mulib_buffer_t *)calloc(count?count:1,sizeof *records);
    if (!rows || !records) { rc=MULIB_SQLITE_NOMEM; goto done; }
    if (i && t->nrows>1) qsort(t->rows,t->nrows,sizeof *t->rows,mulib_sqlite_row_order_);
    for (j=0;j<count;++j) {
      if (!i) {
        mulib_sqlite_table_ *table=&d->tables[j+1];
        rc=mulib_sqlite_value_bytes(&values[0],MULIB_SQLITE_TEXT,"table",5);
        if (!rc) rc=mulib_sqlite_value_bytes(&values[1],MULIB_SQLITE_TEXT,table->name,strlen(table->name));
        if (!rc) rc=mulib_sqlite_value_copy(&values[2],&values[1]);
        values[3].type=MULIB_SQLITE_INTEGER; values[3].integer=roots[j+1];
        if (!rc) rc=mulib_sqlite_value_bytes(&values[4],MULIB_SQLITE_TEXT,table->sql,strlen(table->sql));
        if (!rc) rc=mulib_sqlite_format_encode(values,5,&records[j]);
        for (k=0;k<5;++k) mulib_sqlite_value_clear(&values[k]);
        rows[j].rowid=(int64_t)j+1;
      } else {
        mulib_sqlite_value_t *v=t->rows[j].values,saved={0};
        if (t->primary>=0) { saved=v[t->primary]; memset(&v[t->primary],0,sizeof *v); }
        rc=mulib_sqlite_format_encode(v,t->ncols,&records[j]);
        if (t->primary>=0) v[t->primary]=saved;
        rows[j].rowid=t->rows[j].rowid;
      }
      if (rc) goto done;
      rows[j].record=records[j].data; rows[j].length=records[j].length;
    }
    rc=mulib_sqlite_format_table(&b,roots[i],rows,count); if (rc) goto done;
    for (j=0;j<count;++j) mulib_buffer_free(&records[j]);
    free(records); free(rows); records=NULL; rows=NULL;
  }
  mulib_endian_store_be32(b.image.data+24,d->change+1); mulib_endian_store_be32(b.image.data+40,d->schema);
  mulib_endian_store_be32(b.image.data+60,d->user_version); mulib_endian_store_be32(b.image.data+68,d->application_id);
  mulib_sqlite_format_finish(&b); *out=b.image; memset(&b.image,0,sizeof b.image); rc=0;
done:
  if (records) { for (j=0;j<count;++j) mulib_buffer_free(&records[j]); free(records); }
  free(rows); free(roots); mulib_sqlite_format_builder_free(&b); return rc;
}
static void mulib_sqlite_model_take_(mulib_sqlite_t *to,mulib_sqlite_t *from) {
  mulib_sqlite_tables_free_(to); to->tables=from->tables; to->ntables=from->ntables; from->tables=NULL; from->ntables=0;
  to->page_size=from->page_size; to->change=from->change; to->schema=from->schema;
  to->user_version=from->user_version; to->application_id=from->application_id; to->blocked=from->blocked;
}
static int mulib_sqlite_validate_expr_(mulib_sqlite_eval_ctx_ *e,const mulib_sqlite_node_ *n,int aggregates) {
  const mulib_sqlite_node_ *c; int aggregate=0,rc;
  if (!n) return 0;
  if (n->kind==MULIB_SQ_COLUMN_) {
    c=n->child; if (c->next) { if (!e->table || mulib_sqlite_case_(e->table->name,c->text)) return MULIB_SQLITE_ERROR; c=c->next; }
    return mulib_sqlite_column_index_(e->table,c->text)==-2?MULIB_SQLITE_ERROR:0;
  }
  if (n->kind==MULIB_SQ_CALL_) {
    const char *name=n->child->text; const mulib_sqlite_node_ *args=n->child->next; size_t count=0;
    if (args && args->kind==MULIB_SQ_ARGS_) args=args->child;
    for (c=args;c;c=c->next) ++count;
    aggregate=mulib_sqlite_aggregate_name_(name);
    if (aggregate && !aggregates) return MULIB_SQLITE_ERROR;
    if (!aggregate && mulib_sqlite_case_(name,"coalesce") && mulib_sqlite_case_(name,"ifnull") &&
        mulib_sqlite_case_(name,"typeof") && mulib_sqlite_case_(name,"length") && mulib_sqlite_case_(name,"abs") &&
        mulib_sqlite_case_(name,"upper") && mulib_sqlite_case_(name,"lower")) return MULIB_SQLITE_UNSUPPORTED;
    if (!mulib_sqlite_case_(name,"coalesce")) { if (count<2) return MULIB_SQLITE_ERROR; }
    else if (!mulib_sqlite_case_(name,"ifnull")) { if (count!=2) return MULIB_SQLITE_ERROR; }
    else if (count!=1 || (args->kind==MULIB_SQ_STAR_ && mulib_sqlite_case_(name,"count"))) return MULIB_SQLITE_ERROR;
  }
  for (c=n->child;c;c=c->next) { rc=mulib_sqlite_validate_expr_(e,c,aggregate?0:aggregates); if (rc) return rc; }
  return 0;
}
static int mulib_sqlite_result_names_(mulib_sqlite_stmt_t *s,size_t count) {
  if (count>s->db->options.max_columns || count>SIZE_MAX/sizeof *s->names) return MULIB_SQLITE_TOOBIG;
  s->names=(char **)calloc(count?count:1,sizeof *s->names); if (!s->names) return MULIB_SQLITE_NOMEM;
  s->ncols=count; return 0;
}
static int mulib_sqlite_result_row_(mulib_sqlite_stmt_t *s,mulib_sqlite_value_t **out) {
  size_t capacity,total; mulib_sqlite_value_t *v;
  if (s->nrows>=s->db->options.max_rows) return MULIB_SQLITE_TOOBIG;
  if (s->nrows==s->result_capacity) {
    capacity=s->result_capacity?s->result_capacity*2:16;
    if (capacity<s->result_capacity || (s->ncols && capacity>SIZE_MAX/s->ncols/sizeof *v)) return MULIB_SQLITE_TOOBIG;
    total=capacity*s->ncols;
    v=(mulib_sqlite_value_t *)realloc(s->result,(total?total:1)*sizeof *v); if (!v) return MULIB_SQLITE_NOMEM;
    memset(v+s->result_capacity*s->ncols,0,(capacity-s->result_capacity)*s->ncols*sizeof *v);
    s->result=v; s->result_capacity=capacity;
  }
  *out=s->result+s->nrows*s->ncols; ++s->nrows; return 0;
}
static int mulib_sqlite_name_result_(mulib_sqlite_stmt_t *s,size_t i,const char *name,size_t n) {
  s->names[i]=mulib_sqlite_copy_(name,n); return s->names[i]?0:MULIB_SQLITE_NOMEM;
}
static int mulib_sqlite_where_(mulib_sqlite_eval_ctx_ *e,const mulib_sqlite_node_ *where,int *yes) {
  mulib_sqlite_value_t v={0}; int rc;
  *yes=1; if (!where) return 0;
  rc=mulib_sqlite_eval_(e,where->child,&v); if (!rc) *yes=mulib_sqlite_truth_(&v)==1;
  mulib_sqlite_value_clear(&v); return rc;
}
static int mulib_sqlite_select_(mulib_sqlite_stmt_t *s) {
  mulib_sqlite_t *d=s->db; mulib_sqlite_node_ *root=s->root,*from,*projection,*where,*order,*limit,*item,*c,*alias;
  mulib_sqlite_eval_ctx_ e; mulib_sqlite_table_ *t=NULL; mulib_sqlite_row_ dummy={0},**matched=NULL,*extreme_row=NULL;
  mulib_sqlite_value_t *row,*keys=NULL,value={0},swap; unsigned char *keep=NULL;
  const char *name; size_t i,j,k,n=0,ncols=0,norder=0,out=0,match=0,rows,total,offset=0,maximum=SIZE_MAX;
  int rc=0,yes,aggregate=0,distinct,cmp,direction,index;
  memset(&e,0,sizeof e); e.stmt=s;
  from=mulib_sqlite_child_(root,MULIB_SQ_FROM_); where=mulib_sqlite_child_(root,MULIB_SQ_WHERE_);
  order=mulib_sqlite_child_(root,MULIB_SQ_ORDER_); limit=mulib_sqlite_child_(root,MULIB_SQ_LIMIT_);
  projection=mulib_sqlite_child_(root,MULIB_SQ_PROJECTION_); distinct=mulib_sqlite_child_(root,MULIB_SQ_DISTINCT_)!=NULL;
  if (from) { rc=mulib_sqlite_table_name_(from->child,&name); if (rc) return rc; t=mulib_sqlite_find_table_(d,name); if (!t) return MULIB_SQLITE_ERROR; }
  e.table=t;
  for (item=projection->child;item;item=item->next) {
    if (item->child->kind==MULIB_SQ_STAR_) { if (!t) return MULIB_SQLITE_ERROR; ncols+=t->ncols; }
    else { ++ncols; rc=mulib_sqlite_validate_expr_(&e,item->child,1); if (rc) return rc; if (mulib_sqlite_has_aggregate_(item->child)) aggregate=1; }
  }
  rc=mulib_sqlite_result_names_(s,ncols); if (rc) return rc;
  for (item=projection->child;item;item=item->next) {
    if (item->child->kind==MULIB_SQ_STAR_) for (i=0;i<t->ncols;++i) { rc=mulib_sqlite_name_result_(s,n++,t->cols[i].name,strlen(t->cols[i].name)); if (rc) goto done; }
    else {
      alias=mulib_sqlite_child_(item,MULIB_SQ_ALIAS_); c=(mulib_sqlite_node_ *)mulib_sqlite_unwrap_(item->child);
      if (alias) { name=alias->child->text; total=strlen(name); }
      else if (c && c->kind==MULIB_SQ_COLUMN_) { c=c->child; if (c->next) c=c->next; name=c->text; total=strlen(name); }
      else { name=s->source+item->child->offset; total=item->child->length; while (total && strchr(" \t\r\n\f",name[total-1])) --total; }
      rc=mulib_sqlite_name_result_(s,n++,name,total); if (rc) goto done;
    }
  }
  for (item=order?order->child:NULL;item;item=item->next) {
    c=(mulib_sqlite_node_ *)mulib_sqlite_unwrap_(item->child); index=-1;
    if (c && c->kind==MULIB_SQ_NUMBER_ && c->value.type==MULIB_SQLITE_INTEGER) {
      if (c->value.integer<1 || (uint64_t)c->value.integer>s->ncols) { rc=MULIB_SQLITE_ERROR; goto done; }
      continue;
    }
    if (c && c->kind==MULIB_SQ_COLUMN_ && !c->child->next) for (i=0;i<s->ncols;++i) if (!mulib_sqlite_case_(s->names[i],c->child->text)) { index=(int)i; break; }
    if (index<0) { rc=mulib_sqlite_validate_expr_(&e,item->child,aggregate); if (rc) goto done; }
  }
  rc=mulib_sqlite_validate_expr_(&e,where,0); if (rc) goto done;
  rows=t?t->nrows:1;
  matched=(mulib_sqlite_row_ **)calloc(rows?rows:1,sizeof *matched); if (!matched) { rc=MULIB_SQLITE_NOMEM; goto done; }
  for (i=0;i<rows;++i) {
    e.row=t?&t->rows[i]:&dummy; rc=mulib_sqlite_where_(&e,where,&yes); if (rc) goto done;
    if (yes) matched[match++]=e.row;
  }
  if (order) for (item=order->child;item;item=item->next) ++norder;
  total=aggregate?1:match;
  if (norder && total>SIZE_MAX/norder/sizeof *keys) { rc=MULIB_SQLITE_TOOBIG; goto done; }
  if (norder) { keys=(mulib_sqlite_value_t *)calloc(total?total*norder:1,sizeof *keys); if (!keys) { rc=MULIB_SQLITE_NOMEM; goto done; } }
  e.group=matched; e.group_count=match; e.aggregate=aggregate;
  if (aggregate && match) {
    const mulib_sqlite_node_ *extreme=mulib_sqlite_extreme_call_(projection);
    if (extreme) {
      mulib_sqlite_value_t best={0},candidate={0}; int have_best=0;
      mulib_sqlite_eval_ctx_ scalar=e; scalar.aggregate=0;
      for (i=0;i<match;++i) {
        scalar.row=matched[i]; rc=mulib_sqlite_eval_(&scalar,extreme->child->next->child,&candidate);
        if (rc) { mulib_sqlite_value_clear(&best); mulib_sqlite_value_clear(&candidate); goto done; }
        if (candidate.type!=MULIB_SQLITE_NULL) {
          cmp=mulib_sqlite_compare_values_(&candidate,&best);
          if (!have_best || (!mulib_sqlite_case_(extreme->child->text,"min")?cmp<0:cmp>0)) {
            mulib_sqlite_value_clear(&best); best=candidate; memset(&candidate,0,sizeof candidate);
            extreme_row=matched[i]; have_best=1;
          }
        }
        mulib_sqlite_value_clear(&candidate);
      }
      mulib_sqlite_value_clear(&best);
    }
  }
  for (i=0;i<total;++i) {
    e.row=aggregate && extreme_row?extreme_row:match?matched[aggregate?0:i]:NULL;
    rc=mulib_sqlite_result_row_(s,&row); if (rc) goto done;
    n=0;
    for (item=projection->child;item;item=item->next) {
      if (item->child->kind==MULIB_SQ_STAR_) for (j=0;j<t->ncols;++j) { if (e.row) rc=mulib_sqlite_value_copy(&row[n],&e.row->values[j]); ++n; if (rc) goto done; }
      else { rc=mulib_sqlite_eval_(&e,item->child,&row[n++]); if (rc) goto done; }
    }
    j=0;
    for (item=order?order->child:NULL;item;item=item->next,++j) {
      c=(mulib_sqlite_node_ *)mulib_sqlite_unwrap_(item->child); index=-1;
      if (c && c->kind==MULIB_SQ_NUMBER_ && c->value.type==MULIB_SQLITE_INTEGER) {
        if (c->value.integer<1 || (uint64_t)c->value.integer>s->ncols) { rc=MULIB_SQLITE_ERROR; goto done; } index=(int)c->value.integer-1;
      } else if (c && c->kind==MULIB_SQ_COLUMN_ && !c->child->next) for (k=0;k<s->ncols;++k) if (!mulib_sqlite_case_(s->names[k],c->child->text)) { index=(int)k; break; }
      if (index>=0) rc=mulib_sqlite_value_copy(&keys[i*norder+j],&row[index]);
      else { rc=mulib_sqlite_validate_expr_(&e,item->child,aggregate); if (!rc) rc=mulib_sqlite_eval_(&e,item->child,&keys[i*norder+j]); }
      if (rc) goto done;
    }
  }
  /* Stable bounded sorting. Comparator state is connection-local, no globals. */
  for (i=1;i<s->nrows && norder;++i) for (j=i;j>0;--j) {
    rc=mulib_sqlite_tick_(d); if (rc) goto done;
    cmp=0; k=0;
    for (item=order->child;item;item=item->next,++k) {
      cmp=mulib_sqlite_compare_values_(&keys[(j-1)*norder+k],&keys[j*norder+k]);
      c=mulib_sqlite_child_(item,MULIB_SQ_DIRECTION_); direction=c && (s->source[c->offset]=='D' || s->source[c->offset]=='d')?-1:1;
      if (cmp) { cmp*=direction; break; }
    }
    if (cmp<=0) break;
    for (k=0;k<s->ncols;++k) { swap=s->result[(j-1)*s->ncols+k]; s->result[(j-1)*s->ncols+k]=s->result[j*s->ncols+k]; s->result[j*s->ncols+k]=swap; }
    for (k=0;k<norder;++k) { swap=keys[(j-1)*norder+k]; keys[(j-1)*norder+k]=keys[j*norder+k]; keys[j*norder+k]=swap; }
  }
  if (limit) {
    mulib_sqlite_eval_ctx_ scalar; memset(&scalar,0,sizeof scalar); scalar.stmt=s;
    rc=mulib_sqlite_eval_(&scalar,limit->child,&value); if (rc) goto done;
    if (value.type!=MULIB_SQLITE_INTEGER) { rc=MULIB_SQLITE_MISMATCH; goto done; }
    if (value.integer>=0) maximum=(uint64_t)value.integer>SIZE_MAX?SIZE_MAX:(size_t)value.integer;
    mulib_sqlite_value_clear(&value); c=mulib_sqlite_child_(limit,MULIB_SQ_OFFSET_);
    if (c) { rc=mulib_sqlite_eval_(&scalar,c->child,&value); if (rc) goto done; if (value.type!=MULIB_SQLITE_INTEGER) { rc=MULIB_SQLITE_MISMATCH; goto done; } if (value.integer>0) offset=(uint64_t)value.integer>SIZE_MAX?SIZE_MAX:(size_t)value.integer; }
  }
  keep=(unsigned char *)calloc(s->nrows?s->nrows:1,1); if (!keep) { rc=MULIB_SQLITE_NOMEM; goto done; }
  for (i=0;i<s->nrows;++i) {
    keep[i]=1;
    if (distinct) for (j=0;j<i;++j) {
      rc=mulib_sqlite_tick_(d); if (rc) goto done;
      for (k=0;k<s->ncols;++k) if (mulib_sqlite_compare_values_(&s->result[i*s->ncols+k],&s->result[j*s->ncols+k])) break;
      if (k==s->ncols) { keep[i]=0; break; }
    }
  }
  for (i=0;i<s->nrows;++i) {
    yes=keep[i];
    if (yes && offset) { --offset; yes=0; }
    if (yes && out>=maximum) yes=0;
    if (yes) {
      if (out!=i) for (k=0;k<s->ncols;++k) { mulib_sqlite_value_clear(&s->result[out*s->ncols+k]); s->result[out*s->ncols+k]=s->result[i*s->ncols+k]; memset(&s->result[i*s->ncols+k],0,sizeof *s->result); }
      ++out;
    }
  }
  for (i=out*s->ncols;i<s->nrows*s->ncols;++i) mulib_sqlite_value_clear(&s->result[i]);
  s->nrows=out;
done:
  if (keys) { for (i=0;i<total*norder;++i) mulib_sqlite_value_clear(&keys[i]); free(keys); }
  free(matched); free(keep); mulib_sqlite_value_clear(&value); return rc;
}
static int mulib_sqlite_check_row_(mulib_sqlite_t *d,mulib_sqlite_table_ *t,mulib_sqlite_value_t *values,int64_t *id,int generate) {
  size_t i,attempt; int rc,updating=generate<0; int64_t high=t->nrows?t->rows[0].rowid:0;
  if (updating) generate=0;
  for (i=0;i<t->ncols;++i) {
    rc=mulib_sqlite_affinity_(&values[i],t->cols[i].affinity); if (rc) return rc;
    if (t->cols[i].notnull && !t->cols[i].primary && values[i].type==MULIB_SQLITE_NULL) return MULIB_SQLITE_CONSTRAINT;
  }
  if (t->primary>=0) {
    if (values[t->primary].type==MULIB_SQLITE_INTEGER) *id=values[t->primary].integer;
    else if (values[t->primary].type==MULIB_SQLITE_NULL) { if (updating) return MULIB_SQLITE_MISMATCH; generate=1; }
    else return MULIB_SQLITE_MISMATCH;
  }
  if (generate) {
    for (i=0;i<t->nrows;++i) if (t->rows[i].rowid>high) high=t->rows[i].rowid;
    if (high==INT64_MAX) {
      for (attempt=0;attempt<100;++attempt) {
        *id=(int64_t)(mulib_rng_next(&d->rowid_rng)&INT64_MAX); if (!*id) continue;
        for (i=0;i<t->nrows;++i) if (t->rows[i].rowid==*id) break;
        if (i==t->nrows) break;
      }
      if (attempt==100) return MULIB_SQLITE_FULL;
    } else *id=high+1;
  }
  if (t->primary>=0) { mulib_sqlite_value_clear(&values[t->primary]); values[t->primary].type=MULIB_SQLITE_INTEGER; values[t->primary].integer=*id; }
  return 0;
}
static int mulib_sqlite_mutate_(mulib_sqlite_stmt_t *s) {
  mulib_sqlite_t *d=s->db; mulib_sqlite_node_ *root=s->root,*table_node,*columns,*values,*tuple,*n,*c,*where;
  mulib_sqlite_table_ *t=NULL,*grown,new_table; const char *name; size_t i,j,k,nvalues,index,count;
  mulib_sqlite_value_t *row=NULL,*updated=NULL; int *map=NULL,rc=0,yes,generate; int64_t id;
  mulib_sqlite_eval_ctx_ e; memset(&e,0,sizeof e); e.stmt=s; memset(&new_table,0,sizeof new_table);
  table_node=mulib_sqlite_child_(root,MULIB_SQ_TABLE_); rc=mulib_sqlite_table_name_(table_node,&name); if (rc) return rc;
  t=mulib_sqlite_find_table_(d,name);
  if (root->kind==MULIB_SQ_CREATE_) {
    if (t) return mulib_sqlite_child_(root,MULIB_SQ_IFNOT_)?0:MULIB_SQLITE_ERROR;
    { char prefix[8]={0}; for (i=0;i<7 && name[i];++i) prefix[i]=name[i]; if (!mulib_sqlite_case_(prefix,"sqlite_")) return MULIB_SQLITE_ERROR; }
    if (d->ntables>=d->options.max_columns) return MULIB_SQLITE_TOOBIG;
    new_table.name=mulib_sqlite_copy_(name,strlen(name));
    count=root->length; while (count && strchr(" \t\r\n\f",s->source[root->offset+count-1])) --count;
    new_table.sql=mulib_sqlite_copy_(s->source+root->offset,count);
    if (!new_table.name || !new_table.sql) { rc=MULIB_SQLITE_NOMEM; goto create_done; }
    rc=mulib_sqlite_column_defs_(s,&new_table); if (rc) goto create_done;
    grown=(mulib_sqlite_table_ *)realloc(d->tables,(d->ntables+1)*sizeof *grown); if (!grown) { rc=MULIB_SQLITE_NOMEM; goto create_done; }
    d->tables=grown; grown[d->ntables++]=new_table; memset(&new_table,0,sizeof new_table); ++d->schema;
create_done: mulib_sqlite_table_free_(&new_table); return rc;
  }
  if (!t) return root->kind==MULIB_SQ_DROP_ && mulib_sqlite_child_(root,MULIB_SQ_IFEXISTS_)?0:MULIB_SQLITE_ERROR;
  if (t==d->tables) return MULIB_SQLITE_READONLY;
  e.table=t;
  if (root->kind==MULIB_SQ_DROP_) {
    index=(size_t)(t-d->tables); mulib_sqlite_table_free_(t);
    memmove(d->tables+index,d->tables+index+1,(d->ntables-index-1)*sizeof *t); --d->ntables; ++d->schema; return 0;
  }
  d->changes=0;
  if (root->kind==MULIB_SQ_INSERT_) {
    columns=mulib_sqlite_child_(root,MULIB_SQ_COLUMNS_); values=mulib_sqlite_child_(root,MULIB_SQ_VALUES_);
    nvalues=columns?0:t->ncols; if (columns) for (c=columns->child;c;c=c->next) ++nvalues;
    map=(int *)malloc((nvalues?nvalues:1)*sizeof *map); if (!map) return MULIB_SQLITE_NOMEM;
    if (columns) {
      for (c=columns->child,i=0;c;c=c->next,++i) {
        map[i]=mulib_sqlite_column_index_(t,c->text); if (map[i]==-2) { rc=MULIB_SQLITE_ERROR; goto done; }
        if (map[i]==-1 && t->primary>=0) map[i]=t->primary;
        for (j=0;j<i;++j) if (map[i]==map[j]) { rc=MULIB_SQLITE_ERROR; goto done; }
      }
    } else for (i=0;i<nvalues;++i) map[i]=(int)i;
    tuple=values?values->child:NULL;
    do {
      row=(mulib_sqlite_value_t *)calloc(t->ncols,sizeof *row); if (!row) { rc=MULIB_SQLITE_NOMEM; goto done; }
      for (i=0;i<t->ncols;++i) { rc=mulib_sqlite_value_copy(&row[i],&t->cols[i].def); if (rc) goto done; }
      id=0; generate=t->primary<0;
      if (tuple) {
        i=0;
        for (c=tuple->child;c;c=c->next,++i) {
          mulib_sqlite_value_t v={0}; if (i>=nvalues) { rc=MULIB_SQLITE_ERROR; goto done; }
          rc=mulib_sqlite_validate_expr_(&e,c,0); if (rc) goto done;
          rc=mulib_sqlite_eval_(&e,c,&v); if (rc) { mulib_sqlite_value_clear(&v); goto done; }
          if (map[i]<0) { if (v.type!=MULIB_SQLITE_INTEGER && v.type!=MULIB_SQLITE_NULL) rc=MULIB_SQLITE_MISMATCH; id=v.integer; generate=v.type==MULIB_SQLITE_NULL; if (t->primary>=0) { mulib_sqlite_value_clear(&row[t->primary]); row[t->primary]=v; memset(&v,0,sizeof v); } mulib_sqlite_value_clear(&v); }
          else { mulib_sqlite_value_clear(&row[map[i]]); row[map[i]]=v; }
          if (rc) goto done;
        }
        if (i!=nvalues) { rc=MULIB_SQLITE_ERROR; goto done; }
      }
      rc=mulib_sqlite_check_row_(d,t,row,&id,generate); if (rc) goto done;
      for (i=0;i<t->nrows;++i) if (t->rows[i].rowid==id) { rc=MULIB_SQLITE_CONSTRAINT; goto done; }
      rc=mulib_sqlite_append_row_(d,t,id,row); if (rc) goto done; row=NULL; ++d->changes; d->lastrowid=id;
      if (tuple) tuple=tuple->next;
    } while (tuple);
  } else {
    where=mulib_sqlite_child_(root,MULIB_SQ_WHERE_); rc=mulib_sqlite_validate_expr_(&e,where,0); if (rc) goto done;
    if (root->kind==MULIB_SQ_UPDATE_) for (n=root->child;n;n=n->next) if (n->kind==MULIB_SQ_ASSIGN_) {
      if (mulib_sqlite_column_index_(t,n->child->text)==-2) { rc=MULIB_SQLITE_ERROR; goto done; }
      rc=mulib_sqlite_validate_expr_(&e,n->child->next,0); if (rc) goto done;
    }
    for (i=0;i<t->nrows;++i) {
      e.row=&t->rows[i]; rc=mulib_sqlite_where_(&e,where,&yes); if (rc) goto done;
      if (!yes) continue;
      if (root->kind==MULIB_SQ_DELETE_) {
        mulib_sqlite_format_values_free(t->rows[i].values,t->ncols); memmove(t->rows+i,t->rows+i+1,(t->nrows-i-1)*sizeof *t->rows); --t->nrows; --i;
      } else {
        updated=(mulib_sqlite_value_t *)calloc(t->ncols,sizeof *updated); if (!updated) { rc=MULIB_SQLITE_NOMEM; goto done; }
        for (j=0;j<t->ncols;++j) { rc=mulib_sqlite_value_copy(&updated[j],&e.row->values[j]); if (rc) goto done; }
        id=e.row->rowid; generate=-1;
        for (n=root->child;n;n=n->next) if (n->kind==MULIB_SQ_ASSIGN_) {
          int column=mulib_sqlite_column_index_(t,n->child->text); mulib_sqlite_value_t v={0};
          rc=mulib_sqlite_eval_(&e,n->child->next,&v); if (rc) { mulib_sqlite_value_clear(&v); goto done; }
          if (column<0) {
            if (v.type!=MULIB_SQLITE_INTEGER) { mulib_sqlite_value_clear(&v); rc=MULIB_SQLITE_MISMATCH; goto done; }
            id=v.integer; if (t->primary>=0) { mulib_sqlite_value_clear(&updated[t->primary]); updated[t->primary]=v; memset(&v,0,sizeof v); } mulib_sqlite_value_clear(&v);
          } else { mulib_sqlite_value_clear(&updated[column]); updated[column]=v; }
        }
        rc=mulib_sqlite_check_row_(d,t,updated,&id,generate); if (rc) goto done;
        for (k=0;k<t->nrows;++k) if (k!=i && t->rows[k].rowid==id) { rc=MULIB_SQLITE_CONSTRAINT; goto done; }
        mulib_sqlite_format_values_free(t->rows[i].values,t->ncols); t->rows[i].values=updated; updated=NULL; t->rows[i].rowid=id;
      }
      ++d->changes;
    }
  }
done: free(map); mulib_sqlite_format_values_free(row,t->ncols); mulib_sqlite_format_values_free(updated,t->ncols); return rc;
}
static int mulib_sqlite_pragma_(mulib_sqlite_stmt_t *s,int *writing) {
  mulib_sqlite_t *d=s->db; const char *name; mulib_sqlite_node_ *table=s->root->child,*arg=table->next;
  mulib_sqlite_eval_ctx_ e; mulib_sqlite_value_t value={0},*row; mulib_sqlite_table_ *t;
  size_t i; uint32_t number=0; int rc;
  *writing=0; rc=mulib_sqlite_table_name_(table,&name); if (rc) return rc;
  memset(&e,0,sizeof e); e.stmt=s;
  if (!mulib_sqlite_case_(name,"table_info")) {
    static const char *const names[]={"cid","name","type","notnull","dflt_value","pk"};
    const mulib_sqlite_node_ *n=mulib_sqlite_unwrap_(arg);
    if (!n) return MULIB_SQLITE_ERROR;
    if (n->kind==MULIB_SQ_COLUMN_ && !n->child->next) name=n->child->text;
    else { rc=mulib_sqlite_eval_(&e,arg,&value); if (rc) return rc; if (value.type!=MULIB_SQLITE_TEXT || memchr(value.bytes,0,value.length)) { mulib_sqlite_value_clear(&value); return MULIB_SQLITE_ERROR; } name=(const char *)value.bytes; }
    t=mulib_sqlite_find_table_(d,name); mulib_sqlite_value_clear(&value);
    rc=mulib_sqlite_result_names_(s,6); if (rc) return rc;
    for (i=0;i<6;++i) { rc=mulib_sqlite_name_result_(s,i,names[i],strlen(names[i])); if (rc) return rc; }
    for (i=0;t && i<t->ncols;++i) {
      rc=mulib_sqlite_result_row_(s,&row); if (rc) return rc;
      row[0].type=row[3].type=row[5].type=MULIB_SQLITE_INTEGER;
      row[0].integer=(int64_t)i; row[3].integer=t->cols[i].notnull; row[5].integer=t->cols[i].primary;
      rc=mulib_sqlite_value_bytes(&row[1],MULIB_SQLITE_TEXT,t->cols[i].name,strlen(t->cols[i].name));
      if (!rc) rc=mulib_sqlite_value_bytes(&row[2],MULIB_SQLITE_TEXT,t->cols[i].type,strlen(t->cols[i].type));
      if (rc) return rc;
      if (t->cols[i].default_sql) {
        rc=mulib_sqlite_value_bytes(&row[4],MULIB_SQLITE_TEXT,t->cols[i].default_sql,strlen(t->cols[i].default_sql)); if (rc) return rc;
      }
    }
    return 0;
  }
  if (!mulib_sqlite_case_(name,"journal_mode")) {
    if (arg) {
      const mulib_sqlite_node_ *n=mulib_sqlite_unwrap_(arg);
      if (n && n->kind==MULIB_SQ_COLUMN_ && !mulib_sqlite_case_(n->child->text,"delete")) {}
      else { rc=mulib_sqlite_eval_(&e,arg,&value); if (rc) return rc; if (value.type!=MULIB_SQLITE_TEXT || mulib_sqlite_case_((const char *)value.bytes,"delete")) { mulib_sqlite_value_clear(&value); return MULIB_SQLITE_UNSUPPORTED; } mulib_sqlite_value_clear(&value); }
    }
    rc=mulib_sqlite_result_names_(s,1); if (!rc) rc=mulib_sqlite_name_result_(s,0,name,strlen(name));
    if (!rc) rc=mulib_sqlite_result_row_(s,&row);
    if (!rc) rc=mulib_sqlite_value_bytes(row,MULIB_SQLITE_TEXT,"delete",6);
    return rc;
  }
  if (!mulib_sqlite_case_(name,"user_version")) number=d->user_version;
  else if (!mulib_sqlite_case_(name,"application_id")) number=d->application_id;
  else if (!mulib_sqlite_case_(name,"schema_version")) number=d->schema;
  else if (!mulib_sqlite_case_(name,"page_size")) number=d->page_size;
  else return MULIB_SQLITE_UNSUPPORTED;
  if (arg) {
    if (mulib_sqlite_case_(name,"user_version") && mulib_sqlite_case_(name,"application_id")) return MULIB_SQLITE_UNSUPPORTED;
    rc=mulib_sqlite_eval_(&e,arg,&value); if (rc) return rc;
    if (value.type!=MULIB_SQLITE_INTEGER || value.integer<INT32_MIN || value.integer>INT32_MAX) { mulib_sqlite_value_clear(&value); return MULIB_SQLITE_RANGE; }
    *writing=1; number=(uint32_t)value.integer; mulib_sqlite_value_clear(&value);
    if (!mulib_sqlite_case_(name,"user_version")) d->user_version=number; else d->application_id=number;
    return 0;
  }
  rc=mulib_sqlite_result_names_(s,1); if (!rc) rc=mulib_sqlite_name_result_(s,0,name,strlen(name));
  if (!rc) rc=mulib_sqlite_result_row_(s,&row);
  if (!rc) { row->type=MULIB_SQLITE_INTEGER; row->integer=number<=INT32_MAX?(int64_t)number:(int64_t)number-INT64_C(4294967296); }
  return rc;
}
static void mulib_sqlite_save_free_(mulib_sqlite_save_ *save) {
  if (save) { free(save->name); mulib_buffer_free(&save->image); free(save); }
}
static void mulib_sqlite_saves_free_(mulib_sqlite_t *d) {
  while (d->saves) { mulib_sqlite_save_ *save=d->saves; d->saves=save->next; mulib_sqlite_save_free_(save); }
}
static int mulib_sqlite_persist_(mulib_sqlite_t *d) {
  mulib_buffer_t image={0}; int rc=mulib_sqlite_image_(d,&image);
  if (!rc) rc=mulib_sqlite_pager_commit(d->pager,image.data,image.length);
  if (!rc || rc==MULIB_SQLITE_UNCERTAIN) ++d->change;
  else if (rc==MULIB_SQLITE_IO) d->poisoned=1;
  mulib_buffer_free(&image); return rc;
}
static int mulib_sqlite_snapshot_(mulib_sqlite_t *d,mulib_buffer_t *out) {
  if (d->blocked) {
    size_t n; const void *p=mulib_sqlite_pager_data(d->pager,&n);
    return mulib_buffer_append(out,p,n)?0:MULIB_SQLITE_NOMEM;
  }
  return mulib_sqlite_image_(d,out);
}
static int mulib_sqlite_transaction_(mulib_sqlite_stmt_t *s) {
  mulib_sqlite_t *d=s->db; mulib_sqlite_node_ *root=s->root,*id=root->child;
  mulib_sqlite_save_ *save,*target; mulib_buffer_t image={0}; int rc;
  if (root->kind==MULIB_SQ_BEGIN_) {
    if (d->transaction) return MULIB_SQLITE_ERROR;
    rc=mulib_sqlite_snapshot_(d,&d->transaction_image); if (!rc) { d->transaction=1; d->transaction_dirty=0; } return rc;
  }
  if (root->kind==MULIB_SQ_SAVEPOINT_) {
    save=(mulib_sqlite_save_ *)calloc(1,sizeof *save); if (!save) return MULIB_SQLITE_NOMEM;
    save->name=mulib_sqlite_copy_(id->text,strlen(id->text)); if (!save->name) { free(save); return MULIB_SQLITE_NOMEM; }
    rc=mulib_sqlite_snapshot_(d,&save->image); if (rc) { mulib_sqlite_save_free_(save); return rc; }
    save->dirty=d->transaction_dirty;
    if (!d->transaction) {
      if (!mulib_buffer_append(&d->transaction_image,save->image.data,save->image.length)) { mulib_sqlite_save_free_(save); return MULIB_SQLITE_NOMEM; }
      d->transaction=1; d->transaction_dirty=0; save->outer=1;
    }
    save->next=d->saves; d->saves=save; return 0;
  }
  if (!d->transaction) return MULIB_SQLITE_ERROR;
  if (root->kind==MULIB_SQ_COMMIT_) {
    rc=d->transaction_dirty?mulib_sqlite_persist_(d):0; if (rc && rc!=MULIB_SQLITE_UNCERTAIN) return rc;
    d->transaction=0; d->transaction_dirty=0; mulib_buffer_free(&d->transaction_image); mulib_sqlite_saves_free_(d); return rc;
  }
  if (id) {
    for (target=d->saves;target;target=target->next) if (!mulib_sqlite_case_(target->name,id->text)) break;
    if (!target) return MULIB_SQLITE_ERROR;
    if (root->kind==MULIB_SQ_RELEASE_) {
      if (target->outer) { rc=d->transaction_dirty?mulib_sqlite_persist_(d):0; if (rc && rc!=MULIB_SQLITE_UNCERTAIN) return rc; d->transaction=0; d->transaction_dirty=0; mulib_buffer_free(&d->transaction_image); }
      else rc=0;
      do { save=d->saves; d->saves=save->next; if (save==target) { mulib_sqlite_save_free_(save); break; } mulib_sqlite_save_free_(save); } while (d->saves);
      return rc;
    }
    rc=mulib_sqlite_load_image_(d,target->image.data,target->image.length); if (rc) return rc;
    d->transaction_dirty=target->dirty;
    while (d->saves!=target) { save=d->saves; d->saves=save->next; mulib_sqlite_save_free_(save); } return 0;
  }
  rc=mulib_sqlite_load_image_(d,d->transaction_image.data,d->transaction_image.length); if (rc) return rc;
  d->transaction=0; d->transaction_dirty=0; mulib_buffer_free(&d->transaction_image); mulib_sqlite_saves_free_(d); mulib_buffer_free(&image); return 0;
}
static int mulib_sqlite_execute_(mulib_sqlite_stmt_t *s) {
  mulib_sqlite_t *d=s->db,backup,fresh; mulib_buffer_t before={0},after={0};
  int rc,writing=0; int64_t changes=d->changes,rowid=d->lastrowid;
  if (d->poisoned) return MULIB_SQLITE_IO;
  d->steps=0;
  if (s->root->kind==MULIB_SQ_SELECT_) return mulib_sqlite_select_(s);
  if (s->root->kind>=MULIB_SQ_BEGIN_ && s->root->kind<=MULIB_SQ_RELEASE_) return mulib_sqlite_transaction_(s);
  if (s->root->kind==MULIB_SQ_PRAGMA_ && !s->root->child->next) return mulib_sqlite_pragma_(s,&writing);
  if (s->root->kind==MULIB_SQ_PRAGMA_) {
    const char *name; rc=mulib_sqlite_table_name_(s->root->child,&name); if (rc) return rc;
    if (mulib_sqlite_case_(name,"user_version") && mulib_sqlite_case_(name,"application_id")) return mulib_sqlite_pragma_(s,&writing);
  }
  if (d->readonly) return MULIB_SQLITE_READONLY;
  if (d->blocked) return MULIB_SQLITE_UNSUPPORTED;
  memset(&backup,0,sizeof backup); memset(&fresh,0,sizeof fresh);
  backup.options=fresh.options=d->options; backup.grammar=fresh.grammar=d->grammar;
  /* All undo storage is allocated before mutation. Rollback never allocates. */
  rc=mulib_sqlite_image_(d,&before); if (rc) goto done;
  rc=mulib_sqlite_load_image_(&backup,before.data,before.length); if (rc) goto done;
  rc=s->root->kind==MULIB_SQ_PRAGMA_?mulib_sqlite_pragma_(s,&writing):mulib_sqlite_mutate_(s);
  if (!rc) rc=mulib_sqlite_image_(d,&after);
  if (!rc) rc=mulib_sqlite_load_image_(&fresh,after.data,after.length);
  if (!rc && !d->transaction) rc=mulib_sqlite_pager_commit(d->pager,after.data,after.length);
  if (!rc || rc==MULIB_SQLITE_UNCERTAIN) { mulib_sqlite_model_take_(d,&fresh); if (d->transaction) d->transaction_dirty=1; }
  else {
    if (rc==MULIB_SQLITE_IO) d->poisoned=1;
    mulib_sqlite_model_take_(d,&backup); d->changes=changes; d->lastrowid=rowid;
  }
done: mulib_sqlite_tables_free_(&backup); mulib_sqlite_tables_free_(&fresh); mulib_buffer_free(&before); mulib_buffer_free(&after); return rc;
}
int mulib_sqlite_open(const char *path,unsigned flags,const mulib_sqlite_options_t *options,mulib_sqlite_t **out,mulib_sqlite_error_t *error) {
  mulib_sqlite_t *d; const void *image; size_t length; int rc;
  if (error) { memset(error,0,sizeof *error); error->offset=SIZE_MAX; }
  if (!out) return MULIB_SQLITE_MISUSE;
  d=(mulib_sqlite_t *)calloc(1,sizeof *d); if (!d) { if (error) { error->code=MULIB_SQLITE_NOMEM; snprintf(error->message,sizeof error->message,"connection allocation failed"); } return MULIB_SQLITE_NOMEM; }
  if (options) d->options=*options;
  if (!d->options.max_bytes) d->options.max_bytes=64u*1024u*1024u;
  if (!d->options.max_sql_bytes) d->options.max_sql_bytes=1024u*1024u;
  if (!d->options.max_rows) d->options.max_rows=100000;
  if (!d->options.max_columns) d->options.max_columns=2000;
  if (!d->options.max_steps) d->options.max_steps=10000000;
  if (!d->options.parser.max_depth) d->options.parser.max_depth=256;
  if (d->options.parser.max_depth>512) d->options.parser.max_depth=512;
  if (d->options.max_columns>32766 || d->options.max_rows>SIZE_MAX/sizeof(mulib_sqlite_row_)) { rc=MULIB_SQLITE_MISUSE; goto fail; }
  d->readonly=(flags&MULIB_SQLITE_OPEN_READONLY)!=0;
  mulib_sqlite_error_clear_(d);
  rc=mulib_sqlite_build_grammar_(d); if (rc) goto fail;
  rc=mulib_sqlite_pager_open(path,flags,d->options.max_bytes,&d->pager); if (rc) goto fail;
  image=mulib_sqlite_pager_data(d->pager,&length);
  rc=mulib_sqlite_load_image_(d,image,length); if (rc) goto fail;
  *out=d; return 0;
fail:
  if (!d->error.code) mulib_sqlite_fail_(d,rc,SIZE_MAX,rc==MULIB_SQLITE_UNSUPPORTED?"unsupported SQLite storage or schema feature":"cannot open SQLite database");
  if (error) *error=d->error;
  mulib_sqlite_close(d); return rc;
}
int mulib_sqlite_close(mulib_sqlite_t *d) {
  if (!d) return 0;
  if (d->statements) return MULIB_SQLITE_BUSY;
  mulib_sqlite_tables_free_(d); mulib_sqlite_pager_close(d->pager); mulib_peg_destroy(d->grammar);
  mulib_buffer_free(&d->transaction_image); mulib_sqlite_saves_free_(d); free(d); return 0;
}
size_t mulib_sqlite_parameter_count(const mulib_sqlite_stmt_t *s) { return s?s->nparams:0; }
size_t mulib_sqlite_parameter_index(const mulib_sqlite_stmt_t *s,const char *name) {
  size_t i; if (s && name) for (i=1;i<=s->nparams;++i) if (s->parameter_names[i] && !strcmp(s->parameter_names[i],name)) return i; return 0;
}
int mulib_sqlite_bind(mulib_sqlite_stmt_t *s,size_t i,const mulib_sqlite_value_t *v) {
  if (!s || !v || s->executed) return MULIB_SQLITE_MISUSE;
  if (!i || i>s->nparams) return MULIB_SQLITE_RANGE;
  if (v->length>s->db->options.max_bytes) return MULIB_SQLITE_TOOBIG;
  return mulib_sqlite_value_copy(&s->bindings[i-1],v);
}
int mulib_sqlite_bind_int64(mulib_sqlite_stmt_t *s,size_t i,int64_t n) {
  mulib_sqlite_value_t v={0}; v.type=MULIB_SQLITE_INTEGER; v.integer=n; return mulib_sqlite_bind(s,i,&v);
}
int mulib_sqlite_bind_text(mulib_sqlite_stmt_t *s,size_t i,const char *p,size_t n) {
  mulib_sqlite_value_t v={0}; v.type=MULIB_SQLITE_TEXT; v.bytes=(unsigned char *)(void *)p; v.length=n; return mulib_sqlite_bind(s,i,&v);
}
int mulib_sqlite_bind_blob(mulib_sqlite_stmt_t *s,size_t i,const void *p,size_t n) {
  mulib_sqlite_value_t v={0}; v.type=MULIB_SQLITE_BLOB; v.bytes=(unsigned char *)(void *)p; v.length=n; return mulib_sqlite_bind(s,i,&v);
}
int mulib_sqlite_step(mulib_sqlite_stmt_t *s) {
  int rc;
  if (!s) return MULIB_SQLITE_MISUSE;
  if (s->executed && s->state!=MULIB_SQLITE_ROW) return s->state;
  if (!s->executed) {
    mulib_sqlite_error_clear_(s->db); s->executed=1;
    rc=mulib_sqlite_execute_(s);
    if (rc) { s->state=mulib_sqlite_fail_(s->db,rc,s->root->offset,rc==MULIB_SQLITE_UNSUPPORTED?"SQLite feature not implemented":rc==MULIB_SQLITE_CONSTRAINT?"constraint failed":rc==MULIB_SQLITE_NOMEM?"SQL allocation failed":rc==MULIB_SQLITE_UNCERTAIN?"committed; directory durability uncertain":"SQL execution failed"); return rc; }
  }
  if (s->cursor<s->nrows) { ++s->cursor; s->state=MULIB_SQLITE_ROW; }
  else s->state=MULIB_SQLITE_DONE;
  return s->state;
}
size_t mulib_sqlite_column_count(const mulib_sqlite_stmt_t *s) { return s?s->ncols:0; }
const char *mulib_sqlite_column_name(const mulib_sqlite_stmt_t *s,size_t i) { return s && i<s->ncols?s->names[i]:NULL; }
const mulib_sqlite_value_t *mulib_sqlite_column(const mulib_sqlite_stmt_t *s,size_t i) {
  return s && s->state==MULIB_SQLITE_ROW && s->cursor && i<s->ncols?&s->result[(s->cursor-1)*s->ncols+i]:NULL;
}
int mulib_sqlite_reset(mulib_sqlite_stmt_t *s) {
  size_t i; int rc;
  if (!s) return MULIB_SQLITE_MISUSE;
  rc=s->state==MULIB_SQLITE_ROW || s->state==MULIB_SQLITE_DONE?0:s->state;
  for (i=0;i<s->ncols;++i) free(s->names[i]);
  free(s->names); s->names=NULL;
  mulib_sqlite_format_values_free(s->result,s->nrows*s->ncols); s->result=NULL;
  s->nrows=s->ncols=s->cursor=s->result_capacity=0; s->executed=s->state=0; return rc;
}
int mulib_sqlite_clear_bindings(mulib_sqlite_stmt_t *s) {
  size_t i; if (!s || s->executed) return MULIB_SQLITE_MISUSE;
  for (i=0;i<s->nparams;++i) mulib_sqlite_value_clear(&s->bindings[i]);
  return 0;
}
void mulib_sqlite_finalize(mulib_sqlite_stmt_t *s) {
  if (!s) return;
  mulib_sqlite_reset(s); mulib_sqlite_format_values_free(s->bindings,s->nparams); mulib_arena_reset(&s->arena);
  if (s->db) --s->db->statements;
  free(s);
}
int mulib_sqlite_exec(mulib_sqlite_t *d,const char *sql,mulib_sqlite_error_t *error) {
  size_t n,tail,offset=0; mulib_sqlite_stmt_t *s=NULL; int rc=0;
  if (!d || !sql) return MULIB_SQLITE_MISUSE;
  mulib_sqlite_error_clear_(d);
  n=strlen(sql);
  while (offset<n) {
    if (sql[offset]==';') { ++offset; continue; }
    tail=0; rc=mulib_sqlite_prepare(d,sql+offset,n-offset,&s,&tail,error); if (rc) break;
    if (!s) { offset+=tail; break; }
    do { rc=mulib_sqlite_step(s); } while (rc==MULIB_SQLITE_ROW);
    mulib_sqlite_finalize(s); s=NULL;
    if (rc!=MULIB_SQLITE_DONE) break;
    rc=0; if (!tail) { rc=MULIB_SQLITE_ERROR; break; } offset+=tail;
  }
  if (rc && d->error.offset!=SIZE_MAX) d->error.offset+=offset;
  if (error) *error=d->error;
  return rc;
}
const mulib_sqlite_error_t *mulib_sqlite_error(const mulib_sqlite_t *d) { return d?&d->error:NULL; }
int64_t mulib_sqlite_changes(const mulib_sqlite_t *d) { return d?d->changes:0; }
int64_t mulib_sqlite_last_insert_rowid(const mulib_sqlite_t *d) { return d?d->lastrowid:0; }
int mulib_sqlite_autocommit(const mulib_sqlite_t *d) { return !d || !d->transaction; }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
