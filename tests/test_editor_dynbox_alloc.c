/* Exhaustive allocation failure and moving-realloc coverage for the new stack. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct allocation { void *p; size_t n; } allocation;
static allocation records[4096];
static size_t live, calls, fail_at;
static allocation *record(void *p) {
  size_t i;
  for (i = 0; i < sizeof records/sizeof records[0]; ++i) if (records[i].p == p) return &records[i];
  assert(!"untracked allocation"); return NULL;
}
static void *tracked_malloc(size_t n) {
  void *p;
  allocation *r;
  if (++calls == fail_at) return NULL;
  p = malloc(n ? n : 1);
  if (!p) return NULL;
  r = record(NULL); r->p = p; r->n = n; ++live; return p;
}
static void tracked_free(void *p) {
  if (p) { record(p)->p = NULL; --live; free(p); }
}
static void *tracked_calloc(size_t n, size_t size) {
  void *p;
  if (size && n > SIZE_MAX/size) return NULL;
  p = tracked_malloc(n*size); if (p) memset(p,0,n*size); return p;
}
static void *tracked_realloc(void *p, size_t n) {
  allocation *r;
  void *next;
  if (!p) return tracked_malloc(n);
  if (!n) { tracked_free(p); return NULL; }
  if (++calls == fail_at) return NULL;
  r = record(p); next = malloc(n);
  if (!next) return NULL;
  memcpy(next,p,n < r->n ? n : r->n); free(p); r->p = next; r->n = n; return next;
}
#define malloc tracked_malloc
#define calloc tracked_calloc
#define realloc tracked_realloc
#define free tracked_free
#define MULIB_IMPL_DYNBOX
#define MULIB_IMPL_EDBUF
#define MULIB_IMPL_TERMSYN
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_GRID
#define MULIB_IMPL_CSS
#define MULIB_IMPL_XML
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#include "mulib-dynbox.h"
#include "mulib-termsyn.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void buffer_failures(mulib_edbuf_kind_t kind) {
  unsigned char initial[5000], inserted[6000], actual[5000];
  size_t failure, used = 0;
  memset(initial,'a',sizeof initial); initial[1000] = '\n'; memset(inserted,'b',sizeof inserted);
  /* Include each allocation in initial tree/block/gap construction. */
  for (failure = 1; failure < 128; ++failure) {
    mulib_edbuf_t *b;
    size_t begin = calls;
    fail_at = calls + failure; b = mulib_edbuf_create(kind,initial,sizeof initial); fail_at = 0;
    used = calls - begin; mulib_edbuf_destroy(b); assert(!live);
    if (failure > used) break;
  }
  assert(failure < 128);
  /* New edits must not discard redo/marks/revision when preflight fails. */
  for (failure = 1; failure < 128; ++failure) {
    mulib_edbuf_t *b = mulib_edbuf_create(kind,initial,sizeof initial);
    mulib_edbuf_mark_t *m;
    size_t baseline;
    int r;
    uint64_t revision;
    assert(b && !mulib_edbuf_insert(b,0,"x",1) && !mulib_edbuf_undo(b));
    m = mulib_edbuf_mark_create(b,2500,1); assert(m);
    revision = mulib_edbuf_revision(b); baseline = live;
    fail_at = calls + failure; r = mulib_edbuf_replace(b,501,1200,inserted,sizeof inserted); fail_at = 0;
    if (r) {
      assert(r == MULIB_EDBUF_ENOMEM && live == baseline);
      assert(mulib_edbuf_revision(b) == revision && mulib_edbuf_size(b) == sizeof initial);
      assert(mulib_edbuf_mark_offset(m) == 2500 && mulib_edbuf_line_count(b) == 2);
      assert(!mulib_edbuf_read(b,0,actual,sizeof actual) && !memcmp(actual,initial,sizeof actual));
      assert(!mulib_edbuf_redo(b));
    } else assert(mulib_edbuf_redo(b) == MULIB_EDBUF_ENOTFOUND);
    mulib_edbuf_mark_destroy(m); mulib_edbuf_destroy(b); assert(!live);
    if (!r) break;
  }
  assert(failure < 128);
  /* Undo may allocate on tree/table storage; it must retain its history entry. */
  for (failure = 1; failure < 128; ++failure) {
    mulib_edbuf_t *b = mulib_edbuf_create(kind,initial,sizeof initial);
    char *before;
    size_t size, baseline;
    uint64_t revision;
    int r;
    assert(b && !mulib_edbuf_replace(b,501,1200,inserted,sizeof inserted));
    before = mulib_edbuf_copy(b); assert(before); size = mulib_edbuf_size(b);
    revision = mulib_edbuf_revision(b); baseline = live;
    fail_at = calls + failure; r = mulib_edbuf_undo(b); fail_at = 0;
    if (r) {
      char *after;
      assert(live == baseline && mulib_edbuf_size(b) == size && mulib_edbuf_revision(b) == revision);
      after = mulib_edbuf_copy(b); assert(after && !memcmp(before,after,size)); tracked_free(after);
      assert(!mulib_edbuf_undo(b));
    }
    tracked_free(before); mulib_edbuf_destroy(b); assert(!live);
    if (!r) break;
  }
  assert(failure < 128);
}
static void highlighting_failures(void) {
  const char *source = "int x;\n/* comment\nstill comment\nend */\nreturn x;\n";
  size_t failure;
  for (failure = 1; failure < 128; ++failure) {
    mulib_edbuf_t *b = mulib_edbuf_create(MULIB_EDBUF_ROPE,source,strlen(source));
    mulib_termsyn_t *h = mulib_termsyn_create(MULIB_TERMSYN_C);
    size_t baseline, count;
    int r;
    assert(b && h && !mulib_termsyn_update(h,b)); count = mulib_termsyn_span_count(h);
    assert(!mulib_edbuf_erase(b,7,2)); baseline = live;
    fail_at = calls + failure; r = mulib_termsyn_update(h,b); fail_at = 0;
    if (r) {
      assert(live == baseline && mulib_termsyn_revision(h) == 0 && mulib_termsyn_span_count(h) == count);
      assert(!mulib_termsyn_update(h,b));
    }
    mulib_termsyn_destroy(h); mulib_edbuf_destroy(b); assert(!live);
    if (!r) break;
  }
  assert(failure < 128);
}
static void style_failures(void) {
  const char *css = "code {color:#abc} .keyword {font-weight:bold; color:red}";
  size_t failure;
  for (failure = 1; failure < 256; ++failure) {
    mulib_css_sheet_t *sheet = mulib_css_parse(css,strlen(css),NULL);
    mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT], before[MULIB_TERMSYN_TOKEN_COUNT];
    size_t baseline;
    int r;
    assert(sheet); memset(styles,0x5a,sizeof styles); memcpy(before,styles,sizeof styles); baseline = live;
    fail_at = calls + failure; r = mulib_termsyn_theme(sheet,MULIB_TERMSYN_C,styles,NULL); fail_at = 0;
    if (r) assert(live == baseline && !memcmp(styles,before,sizeof styles));
    mulib_css_destroy(sheet); assert(!live);
    if (!r) break;
  }
  assert(failure < 256);
  for (failure = 1; failure < 64; ++failure) {
    mulib_strbuf_t out = {NULL,0,0};
    mulib_terminfo_t info = {0};
    mulib_term_style_t style = mulib_term_style_default();
    int r;
    size_t baseline;
    style.foreground = MULIB_TERM_RGB(12,34,56); style.attributes = MULIB_TERM_BOLD;
    mulib_terminfo_ansi(&info); assert(mulib_strbuf_append(&out,"sentinel")); baseline = live;
    fail_at = calls + failure;
    r = mulib_termstyle_write(style,&info,MULIB_TERMSTYLE_TRUECOLOR,out.data,out.length,&out);
    fail_at = 0;
    if (r) assert(live == baseline && !strcmp(out.data,"sentinel"));
    mulib_strbuf_free(&out); mulib_terminfo_free(&info); assert(!live);
    if (!r) break;
  }
  assert(failure < 64);
}
static int64_t stack_sum(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f,int64_t g) { return a+b+c+d+e+f+g; }
static void callback(const mulib_dynbox_cif_t *c,void *result,void *const *args,void *user) {
  (void)c; (void)user; *(int64_t *)result = *(int64_t *)args[0] + 1;
}
static void dynbox_failures(void) {
  const mulib_dynbox_type_t *types[7];
  const void *args[7];
  int64_t values[7] = {1,2,3,4,5,6,7}, result = 999;
  mulib_dynbox_cif_t *c = NULL;
  mulib_dynbox_closure_t *closure = NULL;
  size_t i, baseline;
  int64_t (*fn)(int64_t);
  mulib_dynbox_fn entry;
  if (!mulib_dynbox_supported()) return;
  for (i = 0; i < 7; ++i) { types[i] = &mulib_dynbox_type_sint64; args[i] = &values[i]; }
  for (i = 1; i <= 2; ++i) {
    fail_at = calls + i;
    assert(mulib_dynbox_prepare(&c,types[0],7,types) == MULIB_DYNBOX_ENOMEM);
    fail_at = 0; assert(!c && !live);
  }
  assert(!mulib_dynbox_prepare(&c,types[0],7,types)); baseline = live;
  fail_at = calls + 1;
  assert(mulib_dynbox_call(c,(mulib_dynbox_fn)stack_sum,&result,sizeof result,args) == MULIB_DYNBOX_ENOMEM);
  fail_at = 0; assert(result == 999 && live == baseline); assert(!mulib_dynbox_destroy(c)); c = NULL;
  assert(!mulib_dynbox_prepare(&c,types[0],1,types)); baseline = live;
  fail_at = calls + 1;
  assert(mulib_dynbox_closure_create(&closure,c,callback,NULL) == MULIB_DYNBOX_ENOMEM);
  fail_at = 0; assert(!closure && live == baseline);
  assert(!mulib_dynbox_closure_create(&closure,c,callback,NULL));
  entry = mulib_dynbox_closure_function(closure); memcpy(&fn,&entry,sizeof fn);
  baseline = calls; fail_at = calls + 1; assert(fn(41) == 42); fail_at = 0;
  assert(calls == baseline); /* Invoking a C11 callback does not allocate. */
  assert(!mulib_dynbox_closure_destroy(closure) && !mulib_dynbox_destroy(c) && !live);
}
int main(void) {
  int kind;
  for (kind = 0; kind < 3; ++kind) buffer_failures((mulib_edbuf_kind_t)kind);
  highlighting_failures(); style_failures(); dynbox_failures(); assert(!live); return 0;
}
