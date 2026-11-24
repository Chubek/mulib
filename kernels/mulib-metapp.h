/**
 * @file mulib-metapp.h
 * @brief C/C++ preprocessing, connected include graphs and MuLIB-linked plugins.
 * Define MULIB_IMPL_METAPP and ARENA, STRBUF, LINKER, EMITTER, ELF, ENDIAN,
 * UTF8, DSU in one C11 translation unit. See the manual for the language/ELF profile.
 */
#ifndef MULIB_METAPP_H
#define MULIB_METAPP_H
#include "mulib-arena.h"
#include "mulib-strbuf.h"
#include "mulib-linker.h"
#include "mulib-elf.h"
#include "mulib-endian.h"
#include "mulib-utf8.h"
#include "mulib-dsu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_metapp mulib_metapp_t;
enum {
  MULIB_METAPP_OK = 0, MULIB_METAPP_INVALID = -1, MULIB_METAPP_MEMORY = -2,
  MULIB_METAPP_LIMIT = -3, MULIB_METAPP_SYNTAX = -4, MULIB_METAPP_IO = -5,
  MULIB_METAPP_PLUGIN = -6, MULIB_METAPP_UNSUPPORTED = -7, MULIB_METAPP_BUSY = -8,
  MULIB_METAPP_NOTFOUND = -9
};
typedef struct mulib_metapp_error {
  int code;
  size_t line, column;
  char file[256], message[256];
} mulib_metapp_error_t;
typedef struct mulib_metapp_options {
  int cplusplus;                 /**< 0: C17, 1: C++20. */
  int trigraphs;                 /**< Explicit phase-one trigraph support. */
  size_t source_bytes, output_bytes, tokens, steps, arena_bytes;
  size_t macros, include_depth, expansion_depth;
  const char *date, *time;        /**< Copied; defaults Jan  1 1970 / 00:00:00. */
  void (*warning) (void *, const mulib_metapp_error_t *);
  void *warning_user;
} mulib_metapp_options_t;
typedef struct mulib_metapp_source {
  const char *data, *name;
  size_t length;
  void (*release) (void *user);   /**< Called exactly once after successful open. */
  void *user;
} mulib_metapp_source_t;
/** @brief Return 0 with a complete source, IO if absent, another negative code on error.
 * name must identify the same file consistently for pragma once. */
typedef int (*mulib_metapp_include_fn) (void *, const char *including_file,
    const char *header, int angled, mulib_metapp_source_t *out);
/** @brief Create an independent macro environment. Zero limits select documented defaults. */
mulib_metapp_t *mulib_metapp_create (const mulib_metapp_options_t *);
/** @brief Release macros, plugins and owned native images; NULL safe, not during callbacks. */
void mulib_metapp_destroy (mulib_metapp_t *);
/** @brief Copy an include-search directory. Quoted includes first search their source directory. */
int mulib_metapp_include_path (mulib_metapp_t *, const char *);
/** @brief Borrow an include resolver/context; NULL restores the bounded native file reader. */
int mulib_metapp_include_resolver (mulib_metapp_t *, mulib_metapp_include_fn, void *);
/** @brief Define using the text following #define, e.g. "MAX(a,b) ((a)>(b)?(a):(b))".
 * Definitions persist across calls. Same-definition repeats are accepted. */
int mulib_metapp_define (mulib_metapp_t *, const char *definition, mulib_metapp_error_t *);
/** @brief Remove a user macro; missing is harmless, predefined names cannot be removed. */
int mulib_metapp_undefine (mulib_metapp_t *, const char *name);
/** @brief Whether a name is defined, including language-specific predefined macros. */
int mulib_metapp_defined (const mulib_metapp_t *, const char *name);
/** @brief Preprocess explicit source bytes. Append output only on success. Macro changes
 * before an error remain; source/diagnostic pointers are never retained. Output
 * preserves preprocessing tokens, not source whitespace. No concurrent/reentrant use. */
int mulib_metapp_process (mulib_metapp_t *, const char *name, const char *source,
    size_t length, mulib_strbuf_t *out, mulib_metapp_error_t *);
/** @brief Read and preprocess one native file with the configured source bound. */
int mulib_metapp_file (mulib_metapp_t *, const char *path,
                       mulib_strbuf_t *out, mulib_metapp_error_t *);

typedef struct mulib_metapp_graph mulib_metapp_graph_t;
typedef struct mulib_metapp_dependency {
  size_t from, to;               /**< Including file -> included file. */
} mulib_metapp_dependency_t;
typedef struct mulib_metapp_dependency_options {
  size_t max_files, max_edges;   /**< Zero selects 4096 files / 32768 unique edges. */
  size_t max_entries;            /**< Tree enumeration: zero selects 100000 entries. */
} mulib_metapp_dependency_options_t;
/** @brief Index active includes from a complete list of translation-unit roots.
 * Each root starts with an independent copy of pp's macros/configuration. Plugins
 * and resolver state are borrowed during this call. Custom resolvers open roots
 * with including_file=NULL. Roots are sorted/deduplicated before processing.
 * *out must be NULL; failures leave it NULL. The returned graph owns its data. */
int mulib_metapp_dependencies (mulib_metapp_t *pp, const char *const *roots,
    size_t count, const mulib_metapp_dependency_options_t *,
    mulib_metapp_graph_t **out, mulib_metapp_error_t *);
/** @brief POSIX project-tree index: scan C/C++ sources, then unvisited headers.
 * Hidden entries and symlinks are skipped. Uses native files (custom include
 * resolvers are rejected); see the manual for extensions and traversal limits. */
int mulib_metapp_dependencies_tree (mulib_metapp_t *pp, const char *directory,
    const mulib_metapp_dependency_options_t *, mulib_metapp_graph_t **out,
    mulib_metapp_error_t *);
/** @brief Copy the entire weakly connected component containing file, preserving
 * directed include edges. Any file in that component yields identical sorted
 * nodes, IDs, edges and DOT. Missing files return NOTFOUND. *out must be NULL. */
int mulib_metapp_graph_component (const mulib_metapp_graph_t *, const char *file,
                                  mulib_metapp_graph_t **out);
/** @brief Free an index/component independently of its originating preprocessor. */
void mulib_metapp_graph_destroy (mulib_metapp_graph_t *);
/** @brief Number of files; zero for NULL. IDs follow bytewise filename order. */
size_t mulib_metapp_graph_count (const mulib_metapp_graph_t *);
/** @brief Borrow a file identity until graph destruction; NULL for invalid ID. */
const char *mulib_metapp_graph_file (const mulib_metapp_graph_t *, size_t id);
/** @brief Number of unique directed include edges; zero for NULL. */
size_t mulib_metapp_graph_edge_count (const mulib_metapp_graph_t *);
/** @brief Borrow an edge, ordered by (from,to); NULL for invalid index. */
const mulib_metapp_dependency_t *mulib_metapp_graph_edge (const mulib_metapp_graph_t *, size_t);
/** @brief Append deterministic Graphviz DOT transactionally; no entry-file decoration. */
int mulib_metapp_graph_dot (const mulib_metapp_graph_t *, mulib_strbuf_t *out);

#define MULIB_METAPP_PLUGIN_ABI 1u
typedef enum mulib_metapp_event_kind {
  MULIB_METAPP_DIRECTIVE, MULIB_METAPP_PRAGMA, MULIB_METAPP_IDENTIFIER
} mulib_metapp_event_kind_t;
typedef struct mulib_metapp_event {
  mulib_metapp_event_kind_t kind;
  const char *name, *arguments, *file;
  size_t line, column;
} mulib_metapp_event_t;
typedef struct mulib_metapp_host {
  unsigned abi;
  int (*append) (mulib_strbuf_t *, const char *, size_t);
  int (*define) (mulib_metapp_t *, const char *, mulib_metapp_error_t *);
  int (*undefine) (mulib_metapp_t *, const char *);
  int (*defined) (const mulib_metapp_t *, const char *);
} mulib_metapp_host_t;
typedef struct mulib_metapp_plugin {
  unsigned abi;
  size_t size;
  const char *name;
  /** @brief Return 0 to pass, 1 to handle, negative on error. replacement starts empty.
   * Directive/pragma replacement is preprocessed as source; identifier replacement
   * is rescanned as tokens, with this plugin suppressed on its generated tokens.
   * Define/undefine are permitted only in directive/pragma callbacks. */
  int (*handle) (void *, const mulib_metapp_host_t *, mulib_metapp_t *,
                 const mulib_metapp_event_t *, mulib_strbuf_t *replacement);
  void *user;
  void (*destroy) (void *);
} mulib_metapp_plugin_t;
/** @brief Copy a static plugin descriptor/name, adopting user/destroy only on success.
 * Callback code must remain loaded until context destruction. Registration order
 * determines precedence; the first handler wins. At most 64 plugins. */
int mulib_metapp_plugin (mulib_metapp_t *, const mulib_metapp_plugin_t *);
/** @brief Link an ELF64 AMD64 ET_REL image through mulib-linker (no dlopen). Export a
 * const mulib_metapp_plugin_t named mulib_metapp_plugin_v1. Supports local ABS64,
 * ABS32, PC32/PLT32 relocations; imports/TLS/constructors/GOT are rejected.
 * Linux x86-64 only. Owns the linked image on success, input bytes are borrowed
 * only for this call. Native callbacks run with the host's ordinary privileges. */
int mulib_metapp_plugin_link (mulib_metapp_t *, const void *object, size_t length,
                              mulib_metapp_error_t *);
/** @brief Bounded native object-file reader followed by plugin_link. */
int mulib_metapp_plugin_file (mulib_metapp_t *, const char *path, mulib_metapp_error_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_METAPP) && !defined(MULIB_METAPP_IMPLEMENTED)
#define MULIB_METAPP_IMPLEMENTED
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C" {
#endif
extern int lstat (const char *, struct stat *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(__linux__) && defined(__x86_64__) && !defined(__ILP32__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#define MULIB_METAPP_NATIVE_ 1
#endif
typedef struct mulib_metapp_hide_ {
  size_t id;
  const struct mulib_metapp_hide_ *next;
} mulib_metapp_hide_;
enum { MULIB_METAPP_EMPTY_, MULIB_METAPP_ID_, MULIB_METAPP_NUMBER_,
       MULIB_METAPP_STRING_, MULIB_METAPP_CHAR_, MULIB_METAPP_PUNCT_, MULIB_METAPP_NL_, MULIB_METAPP_HEADER_ };
typedef struct mulib_metapp_token_ {
  const char *text, *file;
  size_t length, line, column;
  int kind, space, paste;
  const mulib_metapp_hide_ *hide;
} mulib_metapp_token_;
typedef struct mulib_metapp_tokens_ {
  mulib_metapp_token_ *v;
  size_t n, cap;
} mulib_metapp_tokens_;
typedef struct mulib_metapp_macro_ {
  char *name, **params;
  size_t count, id;
  int function, variadic;
  mulib_metapp_tokens_ body;
  struct mulib_metapp_macro_ *retired;
} mulib_metapp_macro_;
typedef struct mulib_metapp_extension_ {
  mulib_metapp_plugin_t plugin;
  void *mapping;
  size_t mapped;
} mulib_metapp_extension_;
struct mulib_metapp_graph {
  char **files, *cwd;
  size_t count, capacity, edge_count, edge_capacity, max_files, max_edges;
  mulib_metapp_dependency_t *edges;
  size_t *component;
  int custom;
};
struct mulib_metapp {
  mulib_metapp_options_t options;
  char *date, *time, **paths;
  size_t path_count, macro_count, next_id;
  mulib_metapp_macro_ **macros;
  mulib_metapp_macro_ *retired;
  mulib_metapp_extension_ plugins[64];
  size_t plugin_count;
  mulib_metapp_include_fn include;
  void *include_user;
  mulib_arena_t arena;
  size_t arena_used, tokens, steps, counter, include_level;
  char **once;
  size_t once_count;
  int busy, callback_mutation;
  const char *physical_file;
  mulib_metapp_error_t error;
  mulib_strbuf_t result;
  mulib_metapp_graph_t *dependencies;
};
static char *mulib_metapp_graph_name_ (const mulib_metapp_graph_t *, const char *);
static int mulib_metapp_graph_record_ (mulib_metapp_t *, const char *, const char *);
static int
mulib_metapp_fail_ (mulib_metapp_t *p, int code, const mulib_metapp_token_ *t, const char *message)
{
  if (!p->error.code) {
    p->error.code = code; p->error.line = t ? t->line : 1; p->error.column = t ? t->column : 1;
    (void)snprintf (p->error.file, sizeof (p->error.file), "%s", t && t->file ? t->file : p->physical_file ? p->physical_file : "<metapp>");
    (void)snprintf (p->error.message, sizeof (p->error.message), "%s", message);
  }
  return p->error.code;
}
static void *
mulib_metapp_alloc_ (mulib_metapp_t *p, size_t n)
{
  void *v;
  if (n > SIZE_MAX - sizeof (max_align_t) || n + sizeof (max_align_t) > p->options.arena_bytes - p->arena_used) {
    mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "preprocessing arena limit"); return NULL;
  }
  v = mulib_arena_alloc (&p->arena, n);
  if (!v) mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed");
  else p->arena_used += n + sizeof (max_align_t);
  return v;
}
static char *
mulib_metapp_text_ (mulib_metapp_t *p, const char *s, size_t n)
{
  char *v;
  if (n == SIZE_MAX) { mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "text too large"); return NULL; }
  v = (char *)mulib_metapp_alloc_ (p, n+1);
  if (v) { if (n) memcpy (v, s, n); v[n] = 0; }
  return v;
}
static char *
mulib_metapp_copy_ (const char *s, size_t n)
{
  char *v;
  if (n == SIZE_MAX) return NULL;
  v = (char *)malloc (n+1);
  if (v) { if (n) memcpy (v, s, n); v[n] = 0; }
  return v;
}
static int
mulib_metapp_push_ (mulib_metapp_t *p, mulib_metapp_tokens_ *v, mulib_metapp_token_ t)
{
  if (p->tokens++ >= p->options.tokens) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, &t, "token work limit");
  if (v->n == v->cap) {
    size_t cap = v->cap ? v->cap * 2 : 16;
    mulib_metapp_token_ *next;
    if (cap < v->cap || cap > SIZE_MAX/sizeof (*next)) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, &t, "token capacity overflow");
    next = (mulib_metapp_token_ *)realloc (v->v, cap * sizeof (*next));
    if (!next) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, &t, "allocation failed");
    v->v = next; v->cap = cap;
  }
  v->v[v->n++] = t; return 0;
}
static int
mulib_metapp_eq_ (const mulib_metapp_token_ *t, const char *s)
{
  if (t->length == strlen (s) && !memcmp (t->text, s, t->length)) return 1;
  if (t->kind == MULIB_METAPP_PUNCT_) {
    static const char *const alternate[] = { "%:", "%:%:", "<:", ":>", "<%", "%>" };
    static const char *const canonical[] = { "#", "##", "[", "]", "{", "}" };
    size_t i;
    for (i = 0; i < 6; ++i)
      if (!strcmp (s, canonical[i]) && t->length == strlen (alternate[i]) && !memcmp (t->text, alternate[i], t->length)) return 1;
  }
  return 0;
}
static int
mulib_metapp_alternative_ (const mulib_metapp_token_ *t)
{
  static const char *const names[] = { "and", "and_eq", "bitand", "bitor", "compl", "not", "not_eq", "or", "or_eq", "xor", "xor_eq" };
  size_t i;
  for (i = 0; i < sizeof (names)/sizeof (names[0]); ++i) if (mulib_metapp_eq_ (t, names[i])) return 1;
  return 0;
}
static int mulib_metapp_alpha_ (unsigned char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128; }
static int mulib_metapp_digit_ (unsigned char c) { return c >= '0' && c <= '9'; }
static int mulib_metapp_idchar_ (unsigned char c) { return mulib_metapp_alpha_ (c) || mulib_metapp_digit_ (c); }
static int mulib_metapp_hex_ (unsigned char c)
{ return mulib_metapp_digit_ (c) ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1; }
static int
mulib_metapp_step_ (mulib_metapp_t *p, const mulib_metapp_token_ *t)
{ return p->steps++ >= p->options.steps ? mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, t, "preprocessing step limit") : 0; }
/* Phase one/two output has original byte offsets. Raw-string contents are restored
 * from the original bytes while lexing, as required by C++ phase three. */
static int
mulib_metapp_lex_ (mulib_metapp_t *p, const char *file, const char *src, size_t n, mulib_metapp_tokens_ *out)
{
  char *s;
  size_t *map, i = 0, used = 0, at = 0, original = 0, line = 1, column = 1, bol = out->n;
  int space = 0;
  static const char *const punct[] = { "%:%:", ">>=", "<<=", "...", "->*", "<=>", "##", "->", "++", "--", "<<", ">>",
    "<=", ">=", "==", "!=", "&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "::", ".*", "<:", ":>", "<%", "%>", "%:" };
  if ((!src && n) || n > p->options.source_bytes || n > SIZE_MAX/sizeof (*map)-1)
    return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "invalid or oversized source");
  if (n && (memchr (src, 0, n) || !mulib_utf8_validate ((const unsigned char *)src, n)))
    return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, NULL, "source must be NUL-free UTF-8");
  s = (char *)mulib_metapp_alloc_ (p, n+1); map = (size_t *)mulib_metapp_alloc_ (p, (n+1)*sizeof (*map));
  if (!s || !map) return p->error.code;
  if (n >= 3 && !memcmp (src, "\xef\xbb\xbf", 3)) i = 3;
  while (i < n) {
    size_t start = i;
    char c = src[i++];
    if (p->options.trigraphs && c == '?' && i+1 < n && src[i] == '?') {
      static const char trigraphs[] = "=/'()!<>-";
      const char *q = strchr (trigraphs, src[i+1]);
      if (q) { c = "#\\^[]|{}~"[q-trigraphs]; i += 2; }
    }
    if (c == '\\' && i < n && (src[i] == '\n' || src[i] == '\r')) {
      if (src[i++] == '\r' && i < n && src[i] == '\n') ++i;
      continue;
    }
    if (c == '\r') { c = '\n'; if (i < n && src[i] == '\n') ++i; }
    s[used] = c; map[used++] = start;
  }
  s[used] = 0; map[used] = n;
  while (at < used) {
    mulib_metapp_token_ t;
    size_t start = at, quote = SIZE_MAX, prefix = 0, j;
    int raw = 0;
    unsigned char c = (unsigned char)s[at];
    while (original < map[at]) {
      if (src[original] == '\n') { ++line; column = 1; }
      else if (src[original] == '\r' && (original+1 == n || src[original+1] != '\n')) { ++line; column = 1; }
      else ++column;
      ++original;
    }
    memset (&t, 0, sizeof (t)); t.file = file; t.line = line; t.column = column; t.space = space;
    if (mulib_metapp_step_ (p, &t)) return p->error.code;
    if (c != '\n' && (c == ' ' || c == '\t' || c == '\v' || c == '\f')) { ++at; space = 1; continue; }
    if (c == '/' && at+1 < used && s[at+1] == '/') {
      at += 2; while (at < used && s[at] != '\n') ++at; space = 1; continue;
    }
    if (c == '/' && at+1 < used && s[at+1] == '*') {
      at += 2; while (at+1 < used && !(s[at] == '*' && s[at+1] == '/')) ++at;
      if (at+1 == used || at == used) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated comment");
      at += 2; space = 1; continue;
    }
    if ((c == '<' || c == '"') && ((out->n == bol+2 && mulib_metapp_eq_ (out->v+bol, "#") && mulib_metapp_eq_ (out->v+bol+1, "include"))
        || (out->n >= 2 && mulib_metapp_eq_ (out->v+out->n-2, "__has_include") && mulib_metapp_eq_ (out->v+out->n-1, "(")))) {
      char close = c == '<' ? '>' : '"';
      t.kind = MULIB_METAPP_HEADER_; ++at;
      while (at < used && s[at] != close && s[at] != '\n') ++at;
      if (at == used || s[at] != close) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated header name");
      ++at;
    } else if (c == '\n') { ++at; t.kind = MULIB_METAPP_NL_; }
    else {
      if (c == '"' || c == '\'') quote = at;
      else {
        if (c == 'u' && at+1 < used && s[at+1] == '8') prefix = 2;
        else if (c == 'u' || c == 'U' || c == 'L') prefix = 1;
        j = at+prefix;
        if (p->options.cplusplus && j+1 < used && s[j] == 'R' && s[j+1] == '"') { raw = 1; quote = j+1; }
        else if (prefix && j < used && (s[j] == '"' || s[j] == '\'')) quote = j;
      }
      if (quote != SIZE_MAX) {
        t.kind = s[quote] == '"' ? MULIB_METAPP_STRING_ : MULIB_METAPP_CHAR_;
        if (raw) {
          size_t begin = map[quote], delim = begin+1, end, dlen;
          for (end = delim; end < n && src[end] != '('; ++end)
            if (end-delim == 16 || (unsigned char)src[end] <= 32 || src[end] == '\\' || src[end] == ')')
              return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "invalid raw-string delimiter");
          if (end == n) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated raw string");
          dlen = end-delim; ++end;
          while (end < n) {
            if (src[end] == ')' && n-end >= dlen+2 && !memcmp (src+end+1, src+delim, dlen) && src[end+dlen+1] == '"') break;
            ++end;
          }
          if (end == n) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated raw string");
          end += dlen+2;
          t.length = quote-start + end-begin;
          { char *text = (char *)mulib_metapp_alloc_ (p, t.length+1);
            if (!text) return p->error.code;
            memcpy (text, s+start, quote-start); memcpy (text+quote-start, src+begin, end-begin); text[t.length] = 0; t.text = text; }
          while (at < used && map[at] < end) ++at;
        } else {
          at = quote+1;
          while (at < used && s[at] != s[quote] && s[at] != '\n') {
            if (s[at] == '\\' && at+1 < used) ++at;
            ++at;
          }
          if (at == used || s[at] != s[quote]) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated literal");
          ++at;
        }
        if (p->options.cplusplus && at < used && mulib_metapp_alpha_ ((unsigned char)s[at])) {
          size_t suffix = at;
          while (at < used && mulib_metapp_idchar_ ((unsigned char)s[at])) ++at;
          if (t.text) {
            char *text = (char *)mulib_metapp_alloc_ (p, t.length+at-suffix+1);
            if (!text) return p->error.code;
            memcpy (text, t.text, t.length); memcpy (text+t.length, s+suffix, at-suffix);
            t.length += at-suffix; text[t.length] = 0; t.text = text;
          }
        }
      } else if (mulib_metapp_alpha_ (c) || (c == '\\' && at+1 < used && (s[at+1] == 'u' || s[at+1] == 'U'))) {
        mulib_strbuf_t identifier = {0};
        t.kind = MULIB_METAPP_ID_;
        while (at < used) {
          c = (unsigned char)s[at];
          if (mulib_metapp_idchar_ (c)) {
            if (!mulib_strbuf_putc (&identifier, (char)c)) { mulib_strbuf_free (&identifier); return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, &t, "allocation failed"); }
            ++at;
          } else if (c == '\\' && at+1 < used && (s[at+1] == 'u' || s[at+1] == 'U')) {
            uint32_t value = 0; unsigned char encoded[4]; size_t width = s[at+1] == 'u' ? 4 : 8, bytes;
            if (used-at < width+2) { mulib_strbuf_free (&identifier); return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "short universal character name"); }
            for (j = 0; j < width; ++j) { int digit = mulib_metapp_hex_ ((unsigned char)s[at+2+j]);
              if (digit < 0) { mulib_strbuf_free (&identifier); return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "invalid universal character name"); }
              value = value*16+(unsigned)digit; }
            bytes = mulib_utf8_encode (value, encoded, sizeof (encoded));
            if (!bytes || value < 128) { mulib_strbuf_free (&identifier); return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "invalid identifier codepoint"); }
            if (!mulib_strbuf_append_n (&identifier, (const char *)encoded, bytes)) { mulib_strbuf_free (&identifier); return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, &t, "allocation failed"); }
            at += width+2;
          } else break;
        }
        if (at == start || (at < used && mulib_metapp_idchar_ ((unsigned char)s[at]))) { mulib_strbuf_free (&identifier); return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, &t, "allocation failed"); }
        t.text = mulib_metapp_text_ (p, identifier.data, identifier.length); t.length = identifier.length;
        mulib_strbuf_free (&identifier); if (!t.text) return p->error.code;
      } else if (mulib_metapp_digit_ (c) || (c == '.' && at+1 < used && mulib_metapp_digit_ ((unsigned char)s[at+1]))) {
        t.kind = MULIB_METAPP_NUMBER_; ++at;
        while (at < used) {
          c = (unsigned char)s[at];
          if (mulib_metapp_idchar_ (c) || c == '.' || (c == '\'' && at+1 < used && mulib_metapp_idchar_ ((unsigned char)s[at+1]))) ++at;
          else if ((c == '+' || c == '-') && strchr ("eEpP", s[at-1])) ++at;
          else break;
        }
      } else {
        t.kind = MULIB_METAPP_PUNCT_; ++at;
        for (j = 0; j < sizeof (punct)/sizeof (punct[0]); ++j) {
          size_t len = strlen (punct[j]);
          if (!p->options.cplusplus && (!strcmp (punct[j], "->*") || !strcmp (punct[j], "<=>") || !strcmp (punct[j], "::") || !strcmp (punct[j], ".*"))) continue;
          if (p->options.cplusplus && !strcmp (punct[j], "<:") && used-start >= 3 && s[start+2] == ':'
              && (used-start == 3 || (s[start+3] != ':' && s[start+3] != '>'))) continue;
          if (used-start >= len && !memcmp (s+start, punct[j], len)) { at = start+len; break; }
        }
        if (c < 32 || c == 127) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "invalid control byte");
      }
    }
    if (!t.text) { t.text = s+start; t.length = at-start; }
    if (p->options.cplusplus && t.kind == MULIB_METAPP_ID_ && mulib_metapp_alternative_ (&t)) t.kind = MULIB_METAPP_PUNCT_;
    if (mulib_metapp_push_ (p, out, t)) return p->error.code;
    space = t.kind == MULIB_METAPP_NL_;
    if (space) bol = out->n;
  }
  return 0;
}
static int
mulib_metapp_builtin_ (const mulib_metapp_t *p, const char *name)
{
  return !strcmp (name, "__FILE__") || !strcmp (name, "__LINE__") || !strcmp (name, "__COUNTER__")
    || !strcmp (name, "__DATE__") || !strcmp (name, "__TIME__") || !strcmp (name, "__has_include")
    || !strcmp (name, "_Pragma") || (p->options.cplusplus ? !strcmp (name, "__cplusplus")
       : (!strcmp (name, "__STDC__") || !strcmp (name, "__STDC_VERSION__")));
}
static mulib_metapp_macro_ *
mulib_metapp_find_ (const mulib_metapp_t *p, const char *name, size_t n)
{
  size_t i;
  for (i = 0; i < p->macro_count; ++i)
    if (strlen (p->macros[i]->name) == n && !memcmp (p->macros[i]->name, name, n)) return p->macros[i];
  return NULL;
}
static void
mulib_metapp_macro_free_ (mulib_metapp_macro_ *m)
{
  size_t i;
  if (!m) return;
  free (m->name);
  for (i = 0; i < m->count; ++i) free (m->params[i]);
  for (i = 0; i < m->body.n; ++i) free ((void *)m->body.v[i].text);
  free (m->params); free (m->body.v); free (m);
}
static int
mulib_metapp_param_ (const mulib_metapp_macro_ *m, const mulib_metapp_token_ *t)
{
  size_t i;
  if (t->kind != MULIB_METAPP_ID_) return -1;
  for (i = 0; i < m->count; ++i) if (mulib_metapp_eq_ (t, m->params[i])) return (int)i;
  return -1;
}
static int
mulib_metapp_define_tokens_ (mulib_metapp_t *p, mulib_metapp_token_ *v, size_t n)
{
  mulib_metapp_macro_ *m, *old;
  size_t i = 1, j;
  int rc = 0;
  if (!n || v[0].kind != MULIB_METAPP_ID_) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, n ? v : NULL, "expected macro name");
  m = (mulib_metapp_macro_ *)calloc (1, sizeof (*m));
  if (!m) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, v, "allocation failed");
  m->name = mulib_metapp_copy_ (v[0].text, v[0].length);
  if (!m->name) goto memory;
  if (mulib_metapp_builtin_ (p, m->name) || !strcmp (m->name, "defined") || !strcmp (m->name, "__VA_ARGS__") || !strcmp (m->name, "__VA_OPT__")) {
    rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "reserved macro name"); goto done;
  }
  if (i < n && mulib_metapp_eq_ (v+i, "(") && !v[i].space) {
    m->function = 1; ++i;
    while (i < n && !mulib_metapp_eq_ (v+i, ")")) {
      char *name, **params;
      if (m->count == 127) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, v+i, "macro parameter limit"); goto done; }
      if (mulib_metapp_eq_ (v+i, "...")) { m->variadic = 1; name = mulib_metapp_copy_ ("__VA_ARGS__", 11); }
      else if (v[i].kind == MULIB_METAPP_ID_ && !mulib_metapp_eq_ (v+i, "__VA_ARGS__") && !mulib_metapp_eq_ (v+i, "__VA_OPT__")) name = mulib_metapp_copy_ (v[i].text, v[i].length);
      else { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v+i, "invalid macro parameter"); goto done; }
      if (!name) goto memory;
      for (j = 0; j < m->count; ++j) if (!strcmp (name, m->params[j])) { free (name); rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v+i, "duplicate parameter"); goto done; }
      params = (char **)realloc (m->params, (m->count+1)*sizeof (*params));
      if (!params) { free (name); goto memory; }
      m->params = params; m->params[m->count++] = name; ++i;
      if (i < n && mulib_metapp_eq_ (v+i, ")")) break;
      if (m->variadic || i == n || !mulib_metapp_eq_ (v+i, ",")) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "expected comma or closing parenthesis"); goto done; }
      ++i;
      if (i == n || mulib_metapp_eq_ (v+i, ")")) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "trailing parameter comma"); goto done; }
    }
    if (i == n) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "unterminated parameter list"); goto done; }
    ++i;
  }
  if (i < n && (mulib_metapp_eq_ (v+i, "##") || mulib_metapp_eq_ (v+n-1, "##"))) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v+i, "paste operator at replacement boundary"); goto done; }
  for (j = i; j < n; ++j) {
    mulib_metapp_token_ t = v[j];
    if (m->function && mulib_metapp_eq_ (&t, "#") && (j+1 == n || (mulib_metapp_param_ (m, v+j+1) < 0 && !mulib_metapp_eq_ (v+j+1, "__VA_OPT__")))) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "# requires a parameter or __VA_OPT__"); goto done; }
    if ((mulib_metapp_eq_ (&t, "__VA_ARGS__") || mulib_metapp_eq_ (&t, "__VA_OPT__")) && !m->variadic) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "variadic token in nonvariadic macro"); goto done; }
    if (mulib_metapp_eq_ (&t, "__VA_OPT__")) {
      size_t k, nesting = 1;
      if (j+1 == n || !mulib_metapp_eq_ (v+j+1, "(")) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "__VA_OPT__ requires parentheses"); goto done; }
      for (k = j+2; k < n; ++k) {
        if (mulib_metapp_eq_ (v+k, "__VA_OPT__")) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v+k, "nested __VA_OPT__"); goto done; }
        if (mulib_metapp_eq_ (v+k, "(")) ++nesting;
        else if (mulib_metapp_eq_ (v+k, ")") && !--nesting) break;
      }
      if (k == n) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated __VA_OPT__"); goto done; }
      if (k > j+2 && (mulib_metapp_eq_ (v+j+2, "##") || mulib_metapp_eq_ (v+k-1, "##"))) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "paste at __VA_OPT__ boundary"); goto done; }
    }
    t.paste = mulib_metapp_eq_ (&t, "##");
    if (t.paste && j+1 < n && mulib_metapp_eq_ (v+j+1, "##")) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "consecutive paste operators"); goto done; }
    t.text = mulib_metapp_copy_ (t.text, t.length); t.file = NULL; t.hide = NULL;
    if (!t.text) goto memory;
    if (mulib_metapp_push_ (p, &m->body, t)) { free ((void *)t.text); rc = p->error.code; goto done; }
  }
  if (m->body.n) m->body.v[0].space = 0;
  old = mulib_metapp_find_ (p, m->name, strlen (m->name));
  if (old) {
    int same = old->count == m->count && old->function == m->function && old->variadic == m->variadic && old->body.n == m->body.n;
    for (j = 0; same && j < m->count; ++j) same = !strcmp (old->params[j], m->params[j]);
    for (j = 0; same && j < m->body.n; ++j) same = mulib_metapp_eq_ (&old->body.v[j], m->body.v[j].text) && old->body.v[j].space == m->body.v[j].space;
    if (!same) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "incompatible macro redefinition");
    goto done;
  }
  if (p->macro_count == p->options.macros || p->next_id >= SIZE_MAX/2-1) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, v, "macro limit"); goto done; }
  { mulib_metapp_macro_ **next = (mulib_metapp_macro_ **)realloc (p->macros, (p->macro_count+1)*sizeof (*next));
    if (!next) goto memory;
    p->macros = next; m->id = ++p->next_id; p->macros[p->macro_count++] = m; return 0; }
memory:
  rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, v, "allocation failed");
done:
  mulib_metapp_macro_free_ (m); return rc;
}
static int mulib_metapp_expand_ (mulib_metapp_t *, mulib_metapp_tokens_ *, size_t);
static int mulib_metapp_source_ (mulib_metapp_t *, const char *, const char *, size_t, size_t, int);
static int mulib_metapp_pragma_ (mulib_metapp_t *, mulib_metapp_token_ *, size_t, size_t);
static int mulib_metapp_open_ (mulib_metapp_t *, const char *, const char *, int, mulib_metapp_source_t *);
static int mulib_metapp_header_ (mulib_metapp_t *, mulib_metapp_token_ *, size_t, char **, int *);
static int mulib_metapp_pragma_token_ (mulib_metapp_t *, mulib_metapp_token_ *);
static int
mulib_metapp_hidden_ (const mulib_metapp_hide_ *h, size_t id)
{ for (; h; h = h->next) if (h->id == id) return 1; return 0; }
static const mulib_metapp_hide_ *
mulib_metapp_hide_add_ (mulib_metapp_t *p, const mulib_metapp_hide_ *h, size_t id)
{
  mulib_metapp_hide_ *n;
  if (mulib_metapp_hidden_ (h, id)) return h;
  n = (mulib_metapp_hide_ *)mulib_metapp_alloc_ (p, sizeof (*n));
  if (!n) return h;
  n->id = id; n->next = h; return n;
}
static const mulib_metapp_hide_ *
mulib_metapp_hide_union_ (mulib_metapp_t *p, const mulib_metapp_hide_ *a, const mulib_metapp_hide_ *b)
{ for (; b && !p->error.code; b = b->next) a = mulib_metapp_hide_add_ (p, a, b->id); return a; }
static const mulib_metapp_hide_ *
mulib_metapp_hide_intersect_ (mulib_metapp_t *p, const mulib_metapp_hide_ *a, const mulib_metapp_hide_ *b)
{
  const mulib_metapp_hide_ *h = NULL;
  for (; a && !p->error.code; a = a->next) if (mulib_metapp_hidden_ (b, a->id)) h = mulib_metapp_hide_add_ (p, h, a->id);
  return h;
}
static int
mulib_metapp_replace_ (mulib_metapp_t *p, mulib_metapp_tokens_ *v, size_t at, size_t remove, const mulib_metapp_tokens_ *add)
{
  size_t total;
  mulib_metapp_token_ *next;
  if (!add->n && at+remove < v->n && v->v[at].space) v->v[at+remove].space = 1;
  if (add->n > p->options.tokens || v->n-remove > p->options.tokens-add->n)
    return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, v->v+at, "expanded token limit");
  total = v->n-remove+add->n;
  if (total > v->cap) {
    if (total > SIZE_MAX/sizeof (*next)) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, v->v+at, "token overflow");
    next = (mulib_metapp_token_ *)realloc (v->v, total*sizeof (*next));
    if (!next) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, v->v+at, "allocation failed");
    v->v = next; v->cap = total;
  }
  memmove (v->v+at+add->n, v->v+at+remove, (v->n-at-remove)*sizeof (*v->v));
  if (add->n) memcpy (v->v+at, add->v, add->n*sizeof (*v->v));
  v->n = total; return 0;
}
static int
mulib_metapp_join_tokens_ (mulib_strbuf_t *out, const mulib_metapp_token_ *v, size_t n, int all_spaces)
{
  size_t i;
  for (i = 0; i < n; ++i) {
    if (i && (all_spaces || v[i].space) && !mulib_strbuf_putc (out, ' ')) return 0;
    if (!mulib_strbuf_append_n (out, v[i].text, v[i].length)) return 0;
  }
  return 1;
}
static int
mulib_metapp_quote_ (mulib_metapp_t *p, const char *s, size_t n, mulib_metapp_token_ *t)
{
  mulib_strbuf_t text = {0};
  size_t i;
  int ok = mulib_strbuf_putc (&text, '"');
  for (i = 0; i < n && ok; ++i) {
    if (s[i] == '\n' || s[i] == '\r') {
      ok = mulib_strbuf_append (&text, s[i] == '\n' ? "\\n" : "\\r"); continue;
    }
    if (s[i] == '\\' || s[i] == '"') ok = mulib_strbuf_putc (&text, '\\');
    if (ok) ok = mulib_strbuf_putc (&text, s[i]);
  }
  if (ok) ok = mulib_strbuf_putc (&text, '"');
  if (ok) { t->text = mulib_metapp_text_ (p, text.data, text.length); t->length = text.length; t->kind = MULIB_METAPP_STRING_; }
  mulib_strbuf_free (&text);
  return ok && t->text ? 0 : mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, t, "allocation failed");
}
static int
mulib_metapp_plugin_event_ (mulib_metapp_t *p, mulib_metapp_event_kind_t kind,
    const mulib_metapp_token_ *name, const mulib_metapp_token_ *args, size_t n,
    mulib_strbuf_t *replacement, size_t *plugin)
{
  static const mulib_metapp_host_t host = { MULIB_METAPP_PLUGIN_ABI, mulib_strbuf_append_n,
    mulib_metapp_define, mulib_metapp_undefine, mulib_metapp_defined };
  mulib_metapp_event_t event;
  mulib_strbuf_t argument = {0};
  size_t i;
  int rc = 0, old_mutation = p->callback_mutation;
  event.name = mulib_metapp_text_ (p, name->text, name->length);
  if (!event.name || !mulib_metapp_join_tokens_ (&argument, args, n, 0)) {
    mulib_strbuf_free (&argument); return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, name, "allocation failed");
  }
  event.arguments = argument.data ? argument.data : ""; event.kind = kind;
  event.file = name->file; event.line = name->line; event.column = name->column;
  p->callback_mutation = kind != MULIB_METAPP_IDENTIFIER;
  for (i = 0; i < p->plugin_count; ++i) {
    if (kind == MULIB_METAPP_IDENTIFIER && mulib_metapp_hidden_ (name->hide, SIZE_MAX-i)) continue;
    if (mulib_metapp_step_ (p, name)) { rc = p->error.code; break; }
    rc = p->plugins[i].plugin.handle (p->plugins[i].plugin.user, &host, p, &event, replacement);
    if (p->error.code) { rc = p->error.code; break; }
    if (rc < 0 || rc > 1) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_PLUGIN, name, "plugin callback failed"); break; }
    if (replacement->length > p->options.source_bytes) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, name, "plugin replacement limit"); break; }
    if (rc == 1) { if (plugin) *plugin = i; break; }
    if (replacement->length) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_PLUGIN, name, "unhandled plugin event produced output"); break; }
  }
  p->callback_mutation = old_mutation; mulib_strbuf_free (&argument); return rc;
}
typedef struct mulib_metapp_arguments_ {
  mulib_metapp_tokens_ raw[128], expanded[128];
  unsigned char ready[128];
} mulib_metapp_arguments_;
static int
mulib_metapp_argument_ (mulib_metapp_t *p, mulib_metapp_arguments_ *a, size_t index, size_t depth)
{
  size_t i;
  if (a->ready[index]) return 0;
  a->ready[index] = 1;
  for (i = 0; i < a->raw[index].n; ++i)
    if (mulib_metapp_push_ (p, a->expanded+index, a->raw[index].v[i])) return p->error.code;
  return mulib_metapp_expand_ (p, a->expanded+index, depth+1);
}
static int mulib_metapp_paste_ (mulib_metapp_t *, mulib_metapp_tokens_ *);
static int
mulib_metapp_substitute_ (mulib_metapp_t *p, const mulib_metapp_macro_ *m,
    mulib_metapp_arguments_ *args, const mulib_metapp_token_ *body, size_t n,
    mulib_metapp_tokens_ *out, size_t depth, int vaopt)
{
  size_t i;
  if (depth > p->options.expansion_depth) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, n ? body : NULL, "macro expansion depth");
  for (i = 0; i < n; ++i) {
    mulib_metapp_token_ t = body[i];
    int index = mulib_metapp_param_ (m, &t);
    int stringify = m->variadic && mulib_metapp_eq_ (&t, "#") && i+1 < n && mulib_metapp_eq_ (body+i+1, "__VA_OPT__");
    if (mulib_metapp_step_ (p, &t)) return p->error.code;
    if (stringify) ++i;
    if (stringify || mulib_metapp_eq_ (&t, "__VA_OPT__")) {
      size_t begin = i+2, end = begin, nesting = 1;
      if (vaopt || i+1 == n || !mulib_metapp_eq_ (body+i+1, "(")) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "invalid or nested __VA_OPT__");
      for (; end < n; ++end) { if (mulib_metapp_eq_ (body+end, "(")) ++nesting; else if (mulib_metapp_eq_ (body+end, ")") && !--nesting) break; }
      if (end == n) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated __VA_OPT__");
      if (mulib_metapp_argument_ (p, args, m->count-1, depth)) return p->error.code;
      if (stringify) {
        mulib_metapp_tokens_ contents = {0}; mulib_strbuf_t text = {0}; int rc = 0;
        if (args->expanded[m->count-1].n) rc = mulib_metapp_substitute_ (p, m, args, body+begin, end-begin, &contents, depth+1, 1);
        if (!rc) rc = mulib_metapp_paste_ (p, &contents);
        if (!rc && !mulib_metapp_join_tokens_ (&text, contents.v, contents.n, 0)) rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, &t, "allocation failed");
        if (!rc) rc = mulib_metapp_quote_ (p, text.data, text.length, &t);
        free (contents.v); mulib_strbuf_free (&text);
        if (rc) return rc;
        if (mulib_metapp_push_ (p, out, t)) return p->error.code;
      } else if (args->expanded[m->count-1].n) {
        size_t before = out->n;
        if (mulib_metapp_substitute_ (p, m, args, body+begin, end-begin, out, depth+1, 1)) return p->error.code;
        if (out->n == before) { t.kind = MULIB_METAPP_EMPTY_; t.length = 0; if (mulib_metapp_push_ (p, out, t)) return p->error.code; }
        out->v[before].space = t.space;
      } else { t.kind = MULIB_METAPP_EMPTY_; t.length = 0; if (mulib_metapp_push_ (p, out, t)) return p->error.code; }
      i = end;
    } else if (m->function && mulib_metapp_eq_ (&t, "#")) {
      mulib_strbuf_t text = {0};
      index = ++i < n ? mulib_metapp_param_ (m, body+i) : -1;
      if (index < 0) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "invalid stringify operand");
      if (!mulib_metapp_join_tokens_ (&text, args->raw[index].v, args->raw[index].n, 0)) {
        mulib_strbuf_free (&text); return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, &t, "allocation failed"); }
      if (mulib_metapp_quote_ (p, text.data, text.length, &t)) { mulib_strbuf_free (&text); return p->error.code; }
      mulib_strbuf_free (&text);
      if (mulib_metapp_push_ (p, out, t)) return p->error.code;
    } else if (index >= 0) {
      mulib_metapp_tokens_ *value;
      size_t j;
      int raw = (i && mulib_metapp_eq_ (body+i-1, "##")) || (i+1 < n && mulib_metapp_eq_ (body+i+1, "##"));
      if (!raw && mulib_metapp_argument_ (p, args, (size_t)index, depth)) return p->error.code;
      value = raw ? args->raw+index : args->expanded+index;
      if (!value->n) { t.kind = MULIB_METAPP_EMPTY_; t.length = 0; if (mulib_metapp_push_ (p, out, t)) return p->error.code; }
      for (j = 0; j < value->n; ++j) {
        mulib_metapp_token_ item = value->v[j]; if (!j) item.space = t.space;
        if (mulib_metapp_push_ (p, out, item)) return p->error.code;
      }
    } else if (mulib_metapp_push_ (p, out, t)) return p->error.code;
  }
  return 0;
}
static int
mulib_metapp_paste_ (mulib_metapp_t *p, mulib_metapp_tokens_ *v)
{
  size_t i = 0, count = 0;
  while (i < v->n) {
    if (v->v[i].paste) {
      mulib_metapp_token_ left, right;
      if (!count || i+1 == v->n) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v->v+i, "missing paste operand");
      left = v->v[count-1]; right = v->v[++i];
      if (left.kind == MULIB_METAPP_EMPTY_) left = right;
      else if (right.kind != MULIB_METAPP_EMPTY_) {
        char *bytes;
        mulib_metapp_tokens_ joined = {0};
        size_t length;
        if (right.length > SIZE_MAX-left.length-1) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, &left, "paste size overflow");
        length = left.length+right.length; bytes = (char *)mulib_metapp_alloc_ (p, length+1);
        if (!bytes) return p->error.code;
        memcpy (bytes, left.text, left.length); memcpy (bytes+left.length, right.text, right.length); bytes[length] = 0;
        if (mulib_metapp_lex_ (p, left.file, bytes, length, &joined) || joined.n != 1 || joined.v[0].length != length) {
          free (joined.v); return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &left, "paste must form exactly one preprocessing token"); }
        left.text = joined.v[0].text; left.length = joined.v[0].length; left.kind = joined.v[0].kind;
        left.hide = mulib_metapp_hide_intersect_ (p, left.hide, right.hide); free (joined.v);
      }
      v->v[count-1] = left; ++i;
      v->v[count-1].paste = 0;
    } else v->v[count++] = v->v[i++];
  }
  v->n = 0;
  for (i = 0; i < count; ++i) {
    if (v->v[i].kind != MULIB_METAPP_EMPTY_) v->v[v->n++] = v->v[i];
    else if (i+1 < count && v->v[i].space) v->v[i+1].space = 1;
  }
  return p->error.code;
}
static int
mulib_metapp_macro_expand_ (mulib_metapp_t *p, mulib_metapp_tokens_ *v, size_t at,
                            mulib_metapp_macro_ *m, size_t depth)
{
  mulib_metapp_arguments_ args;
  mulib_metapp_tokens_ result = {0};
  mulib_metapp_token_ call = v->v[at];
  const mulib_metapp_hide_ *hide = call.hide;
  size_t end = at+1, count = 0, i;
  int rc = 0;
  memset (&args, 0, sizeof (args));
  if (m->function) {
    size_t nesting = 0;
    if (end == v->n || !mulib_metapp_eq_ (v->v+end, "(")) return 1;
    end += 1;
    for (; end < v->n; ++end) {
      mulib_metapp_token_ t = v->v[end];
      if (mulib_metapp_eq_ (&t, ")") && !nesting) break;
      if (mulib_metapp_eq_ (&t, "(") ) ++nesting;
      else if (mulib_metapp_eq_ (&t, ")")) --nesting;
      if (!nesting && mulib_metapp_eq_ (&t, ",") && !(m->variadic && count+1 >= m->count)) {
        if (++count == 128) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, &t, "argument limit"); goto done; }
      } else if (mulib_metapp_push_ (p, args.raw+count, t)) { rc = p->error.code; goto done; }
    }
    if (end == v->n) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &call, "unterminated macro invocation"); goto done; }
    if (m->count || count || args.raw[0].n) ++count;
    if (m->variadic && count+1 == m->count) ++count;
    if (count != m->count) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &call, "macro argument count mismatch"); goto done; }
    hide = mulib_metapp_hide_intersect_ (p, hide, v->v[end].hide); ++end;
  }
  hide = mulib_metapp_hide_add_ (p, hide, m->id);
  rc = mulib_metapp_substitute_ (p, m, &args, m->body.v, m->body.n, &result, depth+1, 0);
  if (!rc) rc = mulib_metapp_paste_ (p, &result);
  for (i = 0; !rc && i < result.n; ++i) {
    result.v[i].hide = mulib_metapp_hide_union_ (p, result.v[i].hide, hide);
    result.v[i].file = call.file; result.v[i].line = call.line; result.v[i].column = call.column;
    if (!i) result.v[i].space = call.space;
    rc = p->error.code;
  }
  if (!rc) rc = mulib_metapp_replace_ (p, v, at, end-at, &result);
done:
  for (i = 0; i < 128; ++i) { free (args.raw[i].v); free (args.expanded[i].v); }
  free (result.v); return rc;
}
static int
mulib_metapp_number_token_ (mulib_metapp_t *p, mulib_metapp_token_ *t, uint64_t value)
{
  char text[32];
  int n = snprintf (text, sizeof (text), "%llu", (unsigned long long)value);
  t->text = mulib_metapp_text_ (p, text, (size_t)n); t->length = (size_t)n; t->kind = MULIB_METAPP_NUMBER_;
  return t->text ? 0 : p->error.code;
}
/* Already processed pragma output, retained at its expansion position. */
#define MULIB_METAPP_PRAGMA_TOKEN_ 8
static int
mulib_metapp_expand_ (mulib_metapp_t *p, mulib_metapp_tokens_ *v, size_t depth)
{
  size_t at = 0;
  if (depth > p->options.expansion_depth) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, v->n ? v->v : NULL, "macro expansion depth");
  while (at < v->n && !p->error.code) {
    mulib_metapp_token_ t = v->v[at];
    mulib_metapp_macro_ *m;
    if (mulib_metapp_step_ (p, &t)) break;
    if (t.kind != MULIB_METAPP_ID_) { ++at; continue; }
    m = mulib_metapp_find_ (p, t.text, t.length);
    if (m && !mulib_metapp_hidden_ (t.hide, m->id)) {
      int rc = mulib_metapp_macro_expand_ (p, v, at, m, depth);
      if (rc < 0) return rc;
      if (!rc) continue;
      ++at; continue;
    }
    if (mulib_metapp_eq_ (&t, "__FILE__") || mulib_metapp_eq_ (&t, "__DATE__") || mulib_metapp_eq_ (&t, "__TIME__")) {
      const char *text = mulib_metapp_eq_ (&t, "__FILE__") ? t.file : mulib_metapp_eq_ (&t, "__DATE__") ? p->date : p->time;
      if (mulib_metapp_quote_ (p, text, strlen (text), &t)) return p->error.code;
      v->v[at++] = t; continue;
    }
    if (mulib_metapp_eq_ (&t, "__LINE__") || mulib_metapp_eq_ (&t, "__COUNTER__")
        || (p->options.cplusplus && mulib_metapp_eq_ (&t, "__cplusplus"))
        || (!p->options.cplusplus && (mulib_metapp_eq_ (&t, "__STDC__") || mulib_metapp_eq_ (&t, "__STDC_VERSION__")))) {
      uint64_t number = mulib_metapp_eq_ (&t, "__LINE__") ? t.line : mulib_metapp_eq_ (&t, "__COUNTER__") ? p->counter++ :
        mulib_metapp_eq_ (&t, "__cplusplus") ? 202002 : mulib_metapp_eq_ (&t, "__STDC__") ? 1 : 201710;
      if (mulib_metapp_number_token_ (p, &t, number)) return p->error.code;
      if (mulib_metapp_eq_ (&v->v[at], "__cplusplus") || mulib_metapp_eq_ (&v->v[at], "__STDC_VERSION__")) {
        char *text = (char *)mulib_metapp_alloc_ (p, t.length+2);
        if (!text) return p->error.code;
        memcpy (text, t.text, t.length); text[t.length++] = 'L'; text[t.length] = 0; t.text = text;
      }
      v->v[at++] = t; continue;
    }
    if (mulib_metapp_eq_ (&t, "__has_include") || mulib_metapp_eq_ (&t, "_Pragma")) {
      size_t end = at+2, nesting = 1;
      mulib_metapp_tokens_ argument = {0}, replacement = {0};
      int rc = 0;
      if (at+1 == v->n || !mulib_metapp_eq_ (v->v+at+1, "(")) return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "expected builtin argument list");
      for (; end < v->n; ++end) {
        if (mulib_metapp_eq_ (v->v+end, "(")) ++nesting;
        else if (mulib_metapp_eq_ (v->v+end, ")") && !--nesting) break;
        if (mulib_metapp_push_ (p, &argument, v->v[end])) break;
      }
      if (end == v->n) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "unterminated builtin argument");
      if (!rc && !p->error.code && !(mulib_metapp_eq_ (&t, "__has_include") && argument.n
          && (argument.v[0].kind == MULIB_METAPP_STRING_ || argument.v[0].kind == MULIB_METAPP_HEADER_ || mulib_metapp_eq_ (argument.v, "<"))))
        rc = mulib_metapp_expand_ (p, &argument, depth+1);
      if (!rc && !p->error.code && mulib_metapp_eq_ (&t, "__has_include")) {
        char *header; int angled; mulib_metapp_source_t source;
        rc = mulib_metapp_header_ (p, argument.v, argument.n, &header, &angled);
        if (!rc) {
          memset (&source, 0, sizeof (source)); rc = mulib_metapp_open_ (p, p->physical_file, header, angled, &source);
          if (!rc && source.release) source.release (source.user);
          if (!rc || rc == MULIB_METAPP_IO) rc = mulib_metapp_number_token_ (p, &t, rc == 0);
          else rc = mulib_metapp_fail_ (p, rc, &t, "include probe failed");
        }
      } else if (!rc && !p->error.code) {
        mulib_strbuf_t text = {0}; size_t j, start = argument.n == 1 && argument.v[0].text[0] == 'L' ? 2 : 1;
        if (argument.n != 1 || argument.v[0].kind != MULIB_METAPP_STRING_ || argument.v[0].text[start-1] != '"'
            || argument.v[0].length < 2 || argument.v[0].text[argument.v[0].length-1] != '"')
          rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "_Pragma requires an ordinary or wide string");
        else {
          for (j = start; j+1 < argument.v[0].length; ++j) {
            char c = argument.v[0].text[j];
            if (c == '\\' && j+2 < argument.v[0].length && (argument.v[0].text[j+1] == '\\' || argument.v[0].text[j+1] == '"')) c = argument.v[0].text[++j];
            if (!mulib_strbuf_putc (&text, c)) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, &t, "allocation failed"); break; }
          }
          if (!rc) { t.text = mulib_metapp_text_ (p, text.data, text.length); t.length = text.length; t.kind = MULIB_METAPP_PRAGMA_TOKEN_; }
        }
        mulib_strbuf_free (&text);
        if (!rc && !p->error.code) rc = mulib_metapp_pragma_token_ (p, &t);
      }
      if (!rc && !p->error.code && t.length) rc = mulib_metapp_push_ (p, &replacement, t);
      if (!rc && !p->error.code) rc = mulib_metapp_replace_ (p, v, at, end-at+1, &replacement);
      free (argument.v); free (replacement.v);
      if (rc || p->error.code) return p->error.code ? p->error.code : rc;
      if (t.length) ++at;
      continue;
    }
    if (!m && p->plugin_count) {
      mulib_strbuf_t text = {0};
      mulib_metapp_tokens_ replacement = {0};
      size_t plugin = 0, j;
      int rc = mulib_metapp_plugin_event_ (p, MULIB_METAPP_IDENTIFIER, &t, NULL, 0, &text, &plugin);
      if (rc == 1) {
        rc = mulib_metapp_lex_ (p, t.file, text.data, text.length, &replacement);
        for (j = 0; !rc && j < replacement.n; ++j) {
          replacement.v[j].hide = mulib_metapp_hide_add_ (p, t.hide, SIZE_MAX-plugin);
          replacement.v[j].line = t.line; replacement.v[j].column = t.column;
          if (replacement.v[j].kind == MULIB_METAPP_NL_) { memmove (replacement.v+j, replacement.v+j+1, (--replacement.n-j)*sizeof (*replacement.v)); --j; }
        }
        if (!rc && !p->error.code) rc = mulib_metapp_replace_ (p, v, at, 1, &replacement);
        free (replacement.v); mulib_strbuf_free (&text);
        if (rc || p->error.code) return p->error.code;
        continue;
      }
      mulib_strbuf_free (&text); if (rc < 0) return rc;
    }
    ++at;
  }
  return p->error.code;
}
typedef struct mulib_metapp_integer_ { uint64_t value; int unsign; } mulib_metapp_integer_;
typedef struct mulib_metapp_expression_ {
  mulib_metapp_t *pp;
  const mulib_metapp_token_ *v;
  size_t n, at, depth;
} mulib_metapp_expression_;
static int
mulib_metapp_integer_token_ (mulib_metapp_t *p, const mulib_metapp_token_ *t, mulib_metapp_integer_ *out)
{
  size_t at = 0, digits = 0;
  uint64_t value = 0;
  int base = 10, unsign = 0, longs = 0;
  if (t->kind == MULIB_METAPP_CHAR_) {
    size_t end = t->length-1, count = 0;
    while (at < t->length && t->text[at] != '\'') ++at;
    if (at == t->length || t->text[end] != '\'') goto invalid;
    ++at;
    while (at < end) {
      uint32_t c = (unsigned char)t->text[at++];
      if (c == '\\') {
        if (at == end) goto invalid;
        c = (unsigned char)t->text[at++];
        if (c == 'x' || c == 'u' || c == 'U') {
          uint32_t decoded = 0; size_t width = c == 'u' ? 4 : c == 'U' ? 8 : SIZE_MAX, got = 0;
          while (at < end && got < width && mulib_metapp_hex_ ((unsigned char)t->text[at]) >= 0) {
            unsigned digit = (unsigned)mulib_metapp_hex_ ((unsigned char)t->text[at++]);
            if (decoded > (UINT32_MAX-digit)/16) goto invalid;
            decoded = decoded*16+digit; ++got;
          }
          if (!got || (width != SIZE_MAX && got != width)) goto invalid;
          c = decoded;
        } else if (c >= '0' && c <= '7') {
          size_t j = 1; c -= '0';
          while (j < 3 && at < end && t->text[at] >= '0' && t->text[at] <= '7') { c = c*8+(unsigned)(t->text[at++]-'0'); ++j; }
        } else {
          const char *escapes = "abfnrtv\\\'\"?", *where = strchr (escapes, (int)c);
          static const unsigned char values[] = {7,8,12,10,13,9,11,'\\','\'','"','?'};
          if (!where) goto invalid;
          c = values[where-escapes];
        }
      } else if (c >= 128) {
        size_t used;
        --at;
        if (!mulib_utf8_decode ((const unsigned char *)t->text+at, end-at, &c, &used)) goto invalid;
        at += used;
      }
      if (++count > 8 || (count > 1 && c > 255)) goto invalid;
      value = count == 1 ? c : (value << 8) | c;
    }
    if (!count) goto invalid;
    out->value = value; out->unsign = 0; return 0;
  }
  if (t->kind != MULIB_METAPP_NUMBER_) goto invalid;
  if (t->length >= 2 && t->text[0] == '0' && (t->text[1] == 'x' || t->text[1] == 'X')) { base = 16; at = 2; }
  else if (t->length >= 2 && t->text[0] == '0' && (t->text[1] == 'b' || t->text[1] == 'B')) { base = 2; at = 2; }
  else if (t->length > 1 && t->text[0] == '0') base = 8;
  while (at < t->length) {
    int digit = mulib_metapp_hex_ ((unsigned char)t->text[at]);
    if (t->text[at] == '\'' && digits && at+1 < t->length && mulib_metapp_hex_ ((unsigned char)t->text[at+1]) >= 0) { ++at; continue; }
    if (digit < 0 || digit >= base) break;
    if (value > (UINT64_MAX-(unsigned)digit)/(unsigned)base) goto invalid;
    value = value*(unsigned)base+(unsigned)digit; ++digits; ++at;
  }
  if (!digits) goto invalid;
  while (at < t->length) {
    char c = t->text[at++];
    if ((c == 'u' || c == 'U') && !unsign) unsign = 1;
    else if ((c == 'l' || c == 'L') && !longs) {
      longs = 1;
      if (at < t->length && t->text[at] == c) { ++at; ++longs; }
    } else goto invalid;
  }
  if (value > INT64_MAX) {
    if (base == 10 && !unsign) goto invalid;
    unsign = 1;
  }
  out->value = value; out->unsign = unsign; return 0;
invalid:
  return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, t, "invalid integer constant in conditional expression");
}
static const char *
mulib_metapp_operator_ (const mulib_metapp_expression_ *e, const mulib_metapp_token_ *t)
{
  static const char *const operators[] = { "||", "&&", "|", "^", "&", "==", "!=", "<", ">", "<=", ">=", "<<", ">>", "+", "-", "*", "/", "%", "!", "~", "?", ":", "(", ")" };
  size_t i;
  if (e->pp->options.cplusplus) {
    static const char *const names[] = { "or", "and", "bitor", "xor", "bitand", "not_eq", "not", "compl" };
    static const char *const values[] = { "||", "&&", "|", "^", "&", "!=", "!", "~" };
    for (i = 0; i < sizeof (names)/sizeof (names[0]); ++i) if (mulib_metapp_eq_ (t, names[i])) return values[i];
  }
  for (i = 0; i < sizeof (operators)/sizeof (operators[0]); ++i) if (mulib_metapp_eq_ (t, operators[i])) return operators[i];
  return "";
}
static int
mulib_metapp_precedence_ (const char *op)
{
  if (!strcmp (op, "||")) return 1;
  if (!strcmp (op, "&&")) return 2;
  if (!strcmp (op, "|")) return 3;
  if (!strcmp (op, "^")) return 4;
  if (!strcmp (op, "&")) return 5;
  if (!strcmp (op, "==") || !strcmp (op, "!=")) return 6;
  if (!strcmp (op, "<") || !strcmp (op, ">") || !strcmp (op, "<=") || !strcmp (op, ">=")) return 7;
  if (!strcmp (op, "<<") || !strcmp (op, ">>")) return 8;
  if (!strcmp (op, "+") || !strcmp (op, "-")) return 9;
  if (!strcmp (op, "*") || !strcmp (op, "/") || !strcmp (op, "%")) return 10;
  return 0;
}
static mulib_metapp_integer_
mulib_metapp_expr_ (mulib_metapp_expression_ *e, int minimum, int evaluate)
{
  mulib_metapp_integer_ left = {0,0};
  const mulib_metapp_token_ *token;
  const char *op;
  if (++e->depth > e->pp->options.expansion_depth || e->at == e->n) {
    mulib_metapp_fail_ (e->pp, e->at == e->n ? MULIB_METAPP_SYNTAX : MULIB_METAPP_LIMIT,
      e->n ? e->v+e->n-1 : NULL, "missing expression or expression nesting limit"); --e->depth; return left;
  }
  token = e->v+e->at++; op = mulib_metapp_operator_ (e, token);
  if (mulib_metapp_step_ (e->pp, token)) { --e->depth; return left; }
  if (!strcmp (op, "(") ) {
    left = mulib_metapp_expr_ (e, 0, evaluate);
    if (e->at == e->n || !mulib_metapp_eq_ (e->v+e->at, ")")) mulib_metapp_fail_ (e->pp, MULIB_METAPP_SYNTAX, token, "expected closing parenthesis");
    else ++e->at;
  } else if (!strcmp (op, "+") || !strcmp (op, "-") || !strcmp (op, "!") || !strcmp (op, "~")) {
    left = mulib_metapp_expr_ (e, 11, evaluate);
    if (*op == '!') { left.value = !left.value; left.unsign = 0; }
    else if (*op == '-') left.value = UINT64_C(0)-left.value;
    else if (*op == '~') left.value = ~left.value;
  } else if (token->kind == MULIB_METAPP_ID_ && !*op) {
    left.value = e->pp->options.cplusplus && mulib_metapp_eq_ (token, "true");
  } else (void)mulib_metapp_integer_token_ (e->pp, token, &left);
  while (e->at < e->n && !e->pp->error.code) {
    int prec;
    mulib_metapp_integer_ right;
    uint64_t a, b;
    int unsign, right_evaluate = evaluate;
    token = e->v+e->at; op = mulib_metapp_operator_ (e, token); prec = mulib_metapp_precedence_ (op);
    if (!prec || prec < minimum) break;
    ++e->at;
    if (!strcmp (op, "&&") && !left.value) right_evaluate = 0;
    if (!strcmp (op, "||") && left.value) right_evaluate = 0;
    right = mulib_metapp_expr_ (e, prec+1, right_evaluate);
    a = left.value; b = right.value; unsign = left.unsign || right.unsign;
    if (prec <= 7) {
      uint64_t x = unsign ? a : a ^ (UINT64_C(1)<<63), y = unsign ? b : b ^ (UINT64_C(1)<<63);
      if (!strcmp (op, "||")) left.value = !!a || !!b;
      else if (!strcmp (op, "&&")) left.value = !!a && !!b;
      else if (!strcmp (op, "|")) left.value = a|b;
      else if (!strcmp (op, "^")) left.value = a^b;
      else if (!strcmp (op, "&")) left.value = a&b;
      else if (!strcmp (op, "==")) left.value = a == b;
      else if (!strcmp (op, "!=")) left.value = a != b;
      else if (!strcmp (op, "<")) left.value = x < y;
      else if (!strcmp (op, ">")) left.value = x > y;
      else if (!strcmp (op, "<=")) left.value = x <= y;
      else left.value = x >= y;
      left.unsign = prec >= 3 && prec <= 5 ? unsign : 0;
    } else if (prec == 8) {
      if (evaluate && b >= 64) { mulib_metapp_fail_ (e->pp, MULIB_METAPP_SYNTAX, token, "shift count outside 0..63"); break; }
      if (!evaluate || b >= 64) left.value = 0;
      else if (*op == '<') left.value = a << b;
      else { left.value = a >> b; if (!left.unsign && (a >> 63) && b) left.value |= UINT64_MAX << (64-b); }
    } else {
      left.unsign = unsign;
      if (*op == '+') left.value = a+b;
      else if (*op == '-') left.value = a-b;
      else if (*op == '*') left.value = a*b;
      else if (evaluate) {
        int negative_a = !unsign && (int)(a >> 63), negative_b = !unsign && (int)(b >> 63);
        uint64_t x = negative_a ? UINT64_C(0)-a : a, y = negative_b ? UINT64_C(0)-b : b;
        if (!b || (!unsign && a == (UINT64_C(1)<<63) && b == UINT64_MAX)) {
          mulib_metapp_fail_ (e->pp, MULIB_METAPP_SYNTAX, token, "division by zero or signed division overflow"); break;
        }
        left.value = *op == '/' ? x/y : x%y;
        if ((*op == '/' && negative_a != negative_b) || (*op == '%' && negative_a)) left.value = UINT64_C(0)-left.value;
      } else left.value = 0;
    }
  }
  if (!minimum && e->at < e->n && mulib_metapp_eq_ (e->v+e->at, "?") && !e->pp->error.code) {
    mulib_metapp_integer_ yes, no;
    uint64_t condition = left.value;
    token = e->v+e->at++;
    yes = mulib_metapp_expr_ (e, 0, evaluate && condition != 0);
    if (e->at == e->n || !mulib_metapp_eq_ (e->v+e->at, ":")) mulib_metapp_fail_ (e->pp, MULIB_METAPP_SYNTAX, token, "expected conditional colon");
    else ++e->at;
    no = mulib_metapp_expr_ (e, 0, evaluate && condition == 0);
    left.value = condition ? yes.value : no.value; left.unsign = yes.unsign || no.unsign;
  }
  --e->depth; return left;
}
static int
mulib_metapp_condition_ (mulib_metapp_t *p, mulib_metapp_token_ *v, size_t n, int *truth)
{
  mulib_metapp_tokens_ tokens = {0};
  size_t i;
  int rc = 0;
  for (i = 0; i < n && !rc; ++i) {
    mulib_metapp_token_ t = v[i];
    if (mulib_metapp_eq_ (&t, "defined")) {
      int parenthesized = i+1 < n && mulib_metapp_eq_ (v+i+1, "(");
      char *name;
      i += parenthesized ? 2 : 1;
      if (i == n || v[i].kind != MULIB_METAPP_ID_) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "defined requires an identifier"); break; }
      name = mulib_metapp_text_ (p, v[i].text, v[i].length);
      if (!name) { rc = p->error.code; break; }
      if (parenthesized && (++i == n || !mulib_metapp_eq_ (v+i, ")"))) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, &t, "defined requires closing parenthesis"); break; }
      rc = mulib_metapp_number_token_ (p, &t, (uint64_t)mulib_metapp_defined (p, name));
    }
    if (!rc) rc = mulib_metapp_push_ (p, &tokens, t);
  }
  if (!rc) rc = mulib_metapp_expand_ (p, &tokens, 0);
  if (!rc) {
    mulib_metapp_expression_ e;
    mulib_metapp_integer_ value;
    e.pp = p; e.v = tokens.v; e.n = tokens.n; e.at = e.depth = 0;
    value = mulib_metapp_expr_ (&e, 0, 1);
    if (!p->error.code && e.at != e.n) mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, e.v+e.at, "extra tokens in conditional expression");
    rc = p->error.code; if (!rc) *truth = value.value != 0;
  }
  free (tokens.v); return rc;
}
typedef struct mulib_metapp_owned_source_ { char *data, *name; } mulib_metapp_owned_source_;
static void
mulib_metapp_source_release_ (void *user)
{
  mulib_metapp_owned_source_ *s = (mulib_metapp_owned_source_ *)user;
  free (s->data); free (s->name); free (s);
}
static int
mulib_metapp_read_ (const char *path, size_t limit, mulib_metapp_source_t *out)
{
  FILE *f;
  mulib_metapp_owned_source_ *s;
  size_t used = 0, capacity = limit < 4096 ? limit : 4096;
  int rc = 0;
  if (!path || !*path || limit == SIZE_MAX) return MULIB_METAPP_INVALID;
  f = fopen (path, "rb"); if (!f) return MULIB_METAPP_IO;
  s = (mulib_metapp_owned_source_ *)calloc (1, sizeof (*s));
  if (!s) { fclose (f); return MULIB_METAPP_MEMORY; }
  s->name = mulib_metapp_copy_ (path, strlen (path)); s->data = (char *)malloc (capacity+1);
  if (!s->name || !s->data) rc = MULIB_METAPP_MEMORY;
  while (!rc) {
    size_t got = fread (s->data+used, 1, capacity-used+1, f);
    used += got;
    if (ferror (f)) { rc = MULIB_METAPP_IO; break; }
    if (used > limit) { rc = MULIB_METAPP_LIMIT; break; }
    if (used <= capacity) break;
    { size_t next_capacity = capacity > limit/2 ? limit : capacity*2;
      char *next = (char *)realloc (s->data, next_capacity+1);
      if (!next) { rc = MULIB_METAPP_MEMORY; break; }
      s->data = next; capacity = next_capacity; }
  }
  if (fclose (f) && !rc) rc = MULIB_METAPP_IO;
  if (rc) { mulib_metapp_source_release_ (s); return rc; }
  out->data = s->data; out->name = s->name; out->length = used;
  out->release = mulib_metapp_source_release_; out->user = s; return 0;
}
/* Lexical slash-path canonicalization keeps pragma-once identities stable across
 * ./ and ../ spellings. Custom resolvers can supply inode/content identities. */
static void
mulib_metapp_normalize_ (char *out, const char *path)
{
  size_t n = strlen (path), i = 0, used = 0;
  if (path[0] == '/') { out[used++] = '/'; ++i; }
  while (i < n) {
    size_t begin, len;
    while (i < n && path[i] == '/') ++i;
    begin = i; while (i < n && path[i] != '/') ++i; len = i-begin;
    if (!len || (len == 1 && path[begin] == '.')) continue;
    if (len == 2 && path[begin] == '.' && path[begin+1] == '.') {
      size_t previous = used;
      while (previous && out[previous-1] != '/') --previous;
      if (used > previous && !(used-previous == 2 && out[previous] == '.' && out[previous+1] == '.')) {
        used = previous; if (used > 1) --used; continue;
      }
      if (used == 1 && out[0] == '/') continue;
    }
    if (used && out[used-1] != '/') out[used++] = '/';
    memmove (out+used, path+begin, len); used += len;
  }
  if (!used) out[used++] = '.';
  out[used] = 0;
}
static char *
mulib_metapp_path_ (mulib_metapp_t *p, const char *path)
{
  char *out = (char *)mulib_metapp_alloc_ (p, strlen (path)+2);
  if (out) mulib_metapp_normalize_ (out, path);
  return out;
}
static int
mulib_metapp_open_ (mulib_metapp_t *p, const char *from, const char *header,
                    int angled, mulib_metapp_source_t *out)
{
  size_t i;
  int rc = MULIB_METAPP_IO;
  if (p->include) {
    rc = p->include (p->include_user, from, header, angled, out);
    if (rc > 0) return MULIB_METAPP_INVALID;
    if (!rc && ((!out->data && out->length) || !out->name || !*out->name || out->length > p->options.source_bytes)) {
      if (out->release) out->release (out->user);
      memset (out, 0, sizeof (*out)); return MULIB_METAPP_INVALID;
    }
    return rc;
  }
  if (header[0] == '/') return mulib_metapp_read_ (header, p->options.source_bytes, out);
  for (i = 0; i < p->path_count+1; ++i) {
    mulib_strbuf_t path = {0};
    const char *base;
    size_t length;
    int ok;
    if (!i) {
      const char *slash = from ? strrchr (from, '/') : NULL;
      if (angled) continue;
      base = from ? from : ""; length = slash ? (size_t)(slash-from+1) : 0;
    } else { base = p->paths[i-1]; length = strlen (base); }
    ok = mulib_strbuf_append_n (&path, base, length);
    if (ok && length && base[length-1] != '/') ok = mulib_strbuf_putc (&path, '/');
    if (ok) ok = mulib_strbuf_append (&path, header);
    if (!ok) { mulib_strbuf_free (&path); return MULIB_METAPP_MEMORY; }
    rc = mulib_metapp_read_ (path.data, p->options.source_bytes, out); mulib_strbuf_free (&path);
    if (rc != MULIB_METAPP_IO) break;
  }
  return rc;
}
static int
mulib_metapp_header_ (mulib_metapp_t *p, mulib_metapp_token_ *v, size_t n, char **out, int *angled)
{
  mulib_strbuf_t name = {0};
  size_t i;
  int ok = 1;
  if (n == 1 && v[0].kind == MULIB_METAPP_HEADER_ && v[0].length > 2) {
    *angled = v[0].text[0] == '<'; ok = mulib_strbuf_append_n (&name, v[0].text+1, v[0].length-2);
  } else if (n == 1 && v[0].kind == MULIB_METAPP_STRING_ && v[0].length > 2 && v[0].text[0] == '"' && v[0].text[v[0].length-1] == '"') {
    *angled = 0; ok = mulib_strbuf_append_n (&name, v[0].text+1, v[0].length-2);
  } else if (n >= 3 && mulib_metapp_eq_ (v, "<") && mulib_metapp_eq_ (v+n-1, ">")) {
    *angled = 1;
    for (i = 1; i+1 < n && ok; ++i) {
      if (i > 1 && v[i].space) ok = mulib_strbuf_putc (&name, ' ');
      if (ok) ok = mulib_strbuf_append_n (&name, v[i].text, v[i].length);
    }
  } else return mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, n ? v : NULL, "expected quoted or angle header name");
  if (!ok) { mulib_strbuf_free (&name); return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, v, "allocation failed"); }
  *out = mulib_metapp_text_ (p, name.data, name.length); mulib_strbuf_free (&name);
  return *out ? 0 : p->error.code;
}
static int
mulib_metapp_write_ (mulib_metapp_t *p, const char *bytes, size_t n)
{
  if (n > p->options.output_bytes-p->result.length) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "output size limit");
  if (!mulib_strbuf_append_n (&p->result, bytes, n)) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed");
  return 0;
}
static int
mulib_metapp_once_ (mulib_metapp_t *p)
{
  size_t i;
  char **next;
  for (i = 0; i < p->once_count; ++i) if (!strcmp (p->once[i], p->physical_file)) return 0;
  if (p->once_count >= p->options.tokens || p->once_count >= SIZE_MAX/sizeof (*next)-1)
    return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "include identity limit");
  next = (char **)realloc (p->once, (p->once_count+1)*sizeof (*next));
  if (!next) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed");
  p->once = next; p->once[p->once_count++] = (char *)p->physical_file; return 0;
}
static int
mulib_metapp_pragma_ (mulib_metapp_t *p, mulib_metapp_token_ *v, size_t n, size_t depth)
{
  mulib_strbuf_t replacement = {0};
  int rc = 0;
  if (n == 1 && mulib_metapp_eq_ (v, "once")) return mulib_metapp_once_ (p);
  if (n && p->plugin_count) rc = mulib_metapp_plugin_event_ (p, MULIB_METAPP_PRAGMA, v, v+1, n-1, &replacement, NULL);
  if (rc == 1) rc = mulib_metapp_source_ (p, p->physical_file, replacement.data, replacement.length, depth+1, 0);
  else if (!rc) {
    if (!mulib_strbuf_append (&replacement, "\n#pragma ") || !mulib_metapp_join_tokens_ (&replacement, v, n, 1)
        || !mulib_strbuf_putc (&replacement, '\n')) rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, n ? v : NULL, "allocation failed");
    if (!rc) rc = mulib_metapp_write_ (p, replacement.data, replacement.length);
  }
  mulib_strbuf_free (&replacement); return rc;
}
static int
mulib_metapp_pragma_token_ (mulib_metapp_t *p, mulib_metapp_token_ *t)
{
  mulib_metapp_tokens_ pragma = {0};
  mulib_strbuf_t saved = p->result;
  size_t i;
  int rc = mulib_metapp_lex_ (p, t->file, t->text, t->length, &pragma);
  memset (&p->result, 0, sizeof (p->result));
  for (i = 0; !rc && i < pragma.n; ++i) {
    pragma.v[i].line = t->line; pragma.v[i].column = t->column;
    if (pragma.v[i].kind == MULIB_METAPP_NL_) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, t, "newline in pragma operand");
  }
  if (!rc) rc = mulib_metapp_pragma_ (p, pragma.v, pragma.n, p->include_level);
  if (!rc) {
    char *text = (char *)mulib_metapp_alloc_ (p, p->result.length+2);
    size_t prefix = p->result.length && p->result.data[0] != '\n';
    t->length = p->result.length;
    if (text) {
      text[0] = '\n';
      if (t->length) { memcpy (text+prefix, p->result.data, t->length); t->length += prefix; }
      text[t->length] = 0; t->text = text;
    }
    rc = p->error.code;
  }
  mulib_strbuf_free (&p->result); p->result = saved; free (pragma.v); return rc;
}
static int
mulib_metapp_flush_ (mulib_metapp_t *p, mulib_metapp_tokens_ *tokens, size_t depth)
{
  size_t i;
  int rc = mulib_metapp_expand_ (p, tokens, 0);
  (void)depth;
  for (i = 0; !rc && i < tokens->n; ++i) {
    if (tokens->v[i].kind == MULIB_METAPP_PRAGMA_TOKEN_) {
      rc = mulib_metapp_write_ (p, tokens->v[i].text, tokens->v[i].length);
    } else {
      if (i) rc = mulib_metapp_write_ (p, " ", 1);
      if (!rc) rc = mulib_metapp_write_ (p, tokens->v[i].text, tokens->v[i].length);
    }
  }
  if (!rc && tokens->n) rc = mulib_metapp_write_ (p, "\n", 1);
  tokens->n = 0; return rc;
}
typedef struct mulib_metapp_conditional_ { int parent, active, taken, otherwise; } mulib_metapp_conditional_;
static int
mulib_metapp_source_ (mulib_metapp_t *p, const char *name, const char *source, size_t length, size_t depth, int check_once)
{
  mulib_metapp_tokens_ tokens = {0}, pending = {0};
  mulib_metapp_conditional_ conditionals[128];
  const char *previous_file = p->physical_file, *file, *logical;
  size_t at = 0, nesting = 0, i, previous_level = p->include_level;
  int64_t delta = 0;
  int rc;
  if (depth > p->options.include_depth) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "include/generated-source depth limit");
  if (p->dependencies) {
    char *identity = mulib_metapp_graph_name_ (p->dependencies, name);
    if (!identity) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "dependency identity allocation failed");
    file = mulib_metapp_text_ (p, identity, strlen (identity)); free (identity);
  } else file = p->include ? mulib_metapp_text_ (p, name, strlen (name)) : mulib_metapp_path_ (p, name);
  if (!file) return p->error.code;
  if (p->dependencies && mulib_metapp_graph_record_ (p, check_once ? previous_file : NULL, file)) return p->error.code;
  if (check_once) for (i = 0; i < p->once_count; ++i) if (!strcmp (p->once[i], file)) return 0;
  p->physical_file = file; logical = file; p->include_level = depth;
  rc = mulib_metapp_lex_ (p, file, source, length, &tokens);
  while (!rc && at < tokens.n) {
    size_t end = at, n;
    size_t next_line;
    mulib_metapp_token_ *v;
    int active = !nesting || conditionals[nesting-1].active;
    while (end < tokens.n && tokens.v[end].kind != MULIB_METAPP_NL_) ++end;
    next_line = end < tokens.n ? tokens.v[end].line+1 : tokens.n ? tokens.v[tokens.n-1].line+1 : 1;
    for (i = at; i < end; ++i) {
      int64_t mapped = (int64_t)tokens.v[i].line+delta;
      if (mapped <= 0 || (uint64_t)mapped > SIZE_MAX) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, tokens.v+i, "logical line overflow"); break; }
      tokens.v[i].line = (size_t)mapped; tokens.v[i].file = logical;
    }
    if (rc) break;
    v = tokens.v+at; n = end-at; at = end < tokens.n ? end+1 : end;
    if (!n) continue;
    if (!mulib_metapp_eq_ (v, "#")) {
      if (active) for (i = 0; i < n; ++i) { if (!i && pending.n) v[i].space = 1; if ((rc = mulib_metapp_push_ (p, &pending, v[i]))) break; }
      continue;
    }
    rc = mulib_metapp_flush_ (p, &pending, depth); if (rc) break;
    ++v; --n; if (!n) continue;
    if (mulib_metapp_eq_ (v, "if") || mulib_metapp_eq_ (v, "ifdef") || mulib_metapp_eq_ (v, "ifndef")) {
      int truth = 0;
      if (nesting == 128) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, v, "conditional nesting limit"); break; }
      if (mulib_metapp_eq_ (v, "if")) { if (active) rc = mulib_metapp_condition_ (p, v+1, n-1, &truth); }
      else {
        char *macro;
        if (n != 2 || v[1].kind != MULIB_METAPP_ID_) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "expected one conditional identifier"); break; }
        macro = mulib_metapp_text_ (p, v[1].text, v[1].length); if (!macro) { rc = p->error.code; break; }
        truth = mulib_metapp_defined (p, macro); if (mulib_metapp_eq_ (v, "ifndef")) truth = !truth;
      }
      conditionals[nesting].parent = active; conditionals[nesting].active = active && truth;
      conditionals[nesting].taken = active && truth; conditionals[nesting++].otherwise = 0;
    } else if (mulib_metapp_eq_ (v, "elif") || mulib_metapp_eq_ (v, "elifdef") || mulib_metapp_eq_ (v, "elifndef")) {
      mulib_metapp_conditional_ *c;
      int truth = 0;
      if (!nesting || conditionals[nesting-1].otherwise) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "unmatched elif or elif after else"); break; }
      c = conditionals+nesting-1;
      if (c->parent && !c->taken) {
        if (mulib_metapp_eq_ (v, "elif")) rc = mulib_metapp_condition_ (p, v+1, n-1, &truth);
        else {
          char *macro;
          if (n != 2 || v[1].kind != MULIB_METAPP_ID_) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "expected one elif identifier"); break; }
          macro = mulib_metapp_text_ (p, v[1].text, v[1].length); if (!macro) { rc = p->error.code; break; }
          truth = mulib_metapp_defined (p, macro); if (mulib_metapp_eq_ (v, "elifndef")) truth = !truth;
        }
      }
      c->active = c->parent && !c->taken && truth; c->taken |= c->active;
    } else if (mulib_metapp_eq_ (v, "else")) {
      mulib_metapp_conditional_ *c;
      if (!nesting || n != 1 || conditionals[nesting-1].otherwise) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "invalid else"); break; }
      c = conditionals+nesting-1; c->active = c->parent && !c->taken; c->taken = 1; c->otherwise = 1;
    } else if (mulib_metapp_eq_ (v, "endif")) {
      if (!nesting || n != 1) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "invalid endif"); break; }
      --nesting;
    } else if (!active) continue;
    else if (mulib_metapp_eq_ (v, "define")) rc = mulib_metapp_define_tokens_ (p, v+1, n-1);
    else if (mulib_metapp_eq_ (v, "undef")) {
      char *macro;
      if (n != 2 || v[1].kind != MULIB_METAPP_ID_) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "expected one undef identifier"); break; }
      macro = mulib_metapp_text_ (p, v[1].text, v[1].length); if (!macro) { rc = p->error.code; break; }
      p->callback_mutation = 1; rc = mulib_metapp_undefine (p, macro); p->callback_mutation = 0;
      if (rc) rc = mulib_metapp_fail_ (p, rc, v, "cannot undefine predefined macro");
    } else if (mulib_metapp_eq_ (v, "include") || mulib_metapp_eq_ (v, "line")) {
      mulib_metapp_tokens_ argument = {0};
      int direct = n > 1 && (v[1].kind == MULIB_METAPP_STRING_ || v[1].kind == MULIB_METAPP_HEADER_ || mulib_metapp_eq_ (v+1, "<"));
      for (i = 1; i < n && !rc; ++i) rc = mulib_metapp_push_ (p, &argument, v[i]);
      if (!rc && !(direct && mulib_metapp_eq_ (v, "include"))) rc = mulib_metapp_expand_ (p, &argument, 0);
      if (!rc && mulib_metapp_eq_ (v, "include")) {
        char *header; int angled; mulib_metapp_source_t included;
        rc = mulib_metapp_header_ (p, argument.v, argument.n, &header, &angled);
        if (!rc) {
          memset (&included, 0, sizeof (included)); rc = mulib_metapp_open_ (p, file, header, angled, &included);
          if (rc) rc = mulib_metapp_fail_ (p, rc, v, "cannot open include");
          else {
            rc = mulib_metapp_source_ (p, included.name, included.data, included.length, depth+1, 1);
            if (included.release) included.release (included.user);
          }
        }
      } else if (!rc) {
        uint64_t number = 0;
        if (argument.n < 1 || argument.n > 2 || argument.v[0].kind != MULIB_METAPP_NUMBER_) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "invalid line directive");
        else {
          for (i = 0; i < argument.v[0].length; ++i) {
            unsigned char c = (unsigned char)argument.v[0].text[i];
            if (!mulib_metapp_digit_ (c) || number > (UINT64_C(2147483647)-(c-'0'))/10) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "invalid line number"); break; }
            number = number*10+(c-'0');
          }
          if (!rc && !number) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "line number must be positive");
          if (!rc && argument.n == 2) {
            mulib_metapp_token_ *t = argument.v+1;
            if (t->kind != MULIB_METAPP_STRING_ || t->length < 2 || t->text[0] != '"' || t->text[t->length-1] != '"')
              rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, t, "line filename must be an ordinary string");
            else {
              char *filename = (char *)mulib_metapp_alloc_ (p, t->length); size_t j, used = 0;
              if (!filename) rc = p->error.code;
              else {
                for (j = 1; j+1 < t->length; ++j) {
                  if (t->text[j] == '\\' && j+2 < t->length) {
                    ++j;
                    if (t->text[j] != '\\' && t->text[j] != '"') { rc = mulib_metapp_fail_ (p, MULIB_METAPP_UNSUPPORTED, t, "line filenames support only escaped quotes and backslashes"); break; }
                  }
                  filename[used++] = t->text[j];
                }
                filename[used] = 0; if (!rc) logical = filename;
              }
            }
          }
          if (!rc) delta = (int64_t)number-(int64_t)next_line;
        }
      }
      free (argument.v);
    } else if (mulib_metapp_eq_ (v, "pragma")) rc = mulib_metapp_pragma_ (p, v+1, n-1, depth);
    else if (mulib_metapp_eq_ (v, "error") || mulib_metapp_eq_ (v, "warning")) {
      mulib_strbuf_t message = {0};
      if (!mulib_metapp_join_tokens_ (&message, v+1, n-1, 0)) rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, v, "allocation failed");
      else if (mulib_metapp_eq_ (v, "error")) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, message.data ? message.data : "#error");
      else if (p->options.warning) {
        mulib_metapp_error_t warning;
        memset (&warning, 0, sizeof (warning)); warning.line = v->line; warning.column = v->column;
        (void)snprintf (warning.file, sizeof (warning.file), "%s", v->file);
        (void)snprintf (warning.message, sizeof (warning.message), "%s", message.data ? message.data : "#warning");
        p->options.warning (p->options.warning_user, &warning);
      }
      mulib_strbuf_free (&message);
    } else {
      mulib_strbuf_t replacement = {0};
      rc = p->plugin_count ? mulib_metapp_plugin_event_ (p, MULIB_METAPP_DIRECTIVE, v, v+1, n-1, &replacement, NULL) : 0;
      if (rc == 1) rc = mulib_metapp_source_ (p, file, replacement.data, replacement.length, depth+1, 0);
      else if (!rc) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, v, "unknown preprocessing directive");
      mulib_strbuf_free (&replacement);
    }
  }
  if (!rc && nesting) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, tokens.n ? tokens.v+tokens.n-1 : NULL, "unterminated conditional group");
  if (!rc) rc = mulib_metapp_flush_ (p, &pending, depth);
  free (tokens.v); free (pending.v); p->physical_file = previous_file; p->include_level = previous_level;
  return rc;
}
static void
mulib_metapp_reset_ (mulib_metapp_t *p)
{
  while (p->retired) { mulib_metapp_macro_ *next = p->retired->retired; mulib_metapp_macro_free_ (p->retired); p->retired = next; }
  mulib_arena_reset (&p->arena); p->arena_used = p->tokens = p->steps = p->counter = 0;
  free (p->once); p->once = NULL; p->once_count = 0; p->physical_file = NULL;
  memset (&p->error, 0, sizeof (p->error)); mulib_strbuf_free (&p->result);
}
mulib_metapp_t *
mulib_metapp_create (const mulib_metapp_options_t *options)
{
  mulib_metapp_t *p = (mulib_metapp_t *)calloc (1, sizeof (*p));
  if (!p) return NULL;
  if (options) p->options = *options;
#define MULIB_METAPP_DEFAULT_(field, value) if (!p->options.field) p->options.field = (value)
  MULIB_METAPP_DEFAULT_ (source_bytes, 4*1024*1024);
  MULIB_METAPP_DEFAULT_ (output_bytes, 16*1024*1024);
  MULIB_METAPP_DEFAULT_ (tokens, 1000000);
  MULIB_METAPP_DEFAULT_ (steps, 10000000);
  MULIB_METAPP_DEFAULT_ (arena_bytes, 128*1024*1024);
  MULIB_METAPP_DEFAULT_ (macros, 4096);
  MULIB_METAPP_DEFAULT_ (include_depth, 64);
  MULIB_METAPP_DEFAULT_ (expansion_depth, 128);
#undef MULIB_METAPP_DEFAULT_
  if (p->options.macros > SIZE_MAX/sizeof (*p->macros) || p->options.source_bytes >= (size_t)PTRDIFF_MAX
      || p->options.output_bytes >= (size_t)PTRDIFF_MAX || p->options.tokens >= SIZE_MAX/sizeof (mulib_metapp_token_)
      || p->options.expansion_depth > 512 || p->options.include_depth > 256) { free (p); return NULL; }
  p->date = mulib_metapp_copy_ (options && options->date ? options->date : "Jan  1 1970", strlen (options && options->date ? options->date : "Jan  1 1970"));
  p->time = mulib_metapp_copy_ (options && options->time ? options->time : "00:00:00", strlen (options && options->time ? options->time : "00:00:00"));
  mulib_arena_init (&p->arena, 4096);
  if (!p->date || !p->time || strlen (p->date) != 11 || strlen (p->time) != 8 || strpbrk (p->date, "\n\r") || strpbrk (p->time, "\n\r")) {
    mulib_metapp_destroy (p); return NULL;
  }
  return p;
}
void
mulib_metapp_destroy (mulib_metapp_t *p)
{
  size_t i;
  if (!p || p->busy) return;
  p->busy = 1;
  for (i = p->plugin_count; i; --i) {
    mulib_metapp_extension_ *e = p->plugins+i-1;
    if (e->plugin.destroy) e->plugin.destroy (e->plugin.user);
    free ((void *)e->plugin.name);
#ifdef MULIB_METAPP_NATIVE_
    if (e->mapping) (void)munmap (e->mapping, e->mapped);
#endif
  }
  for (i = 0; i < p->macro_count; ++i) mulib_metapp_macro_free_ (p->macros[i]);
  for (i = 0; i < p->path_count; ++i) free (p->paths[i]);
  free (p->macros); free (p->paths); free (p->date); free (p->time);
  mulib_metapp_reset_ (p); free (p);
}
int
mulib_metapp_include_path (mulib_metapp_t *p, const char *path)
{
  char *copy, **next;
  if (!p || !path || !*path) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  if (p->path_count == 256) return MULIB_METAPP_LIMIT;
  copy = mulib_metapp_copy_ (path, strlen (path)); if (!copy) return MULIB_METAPP_MEMORY;
  next = (char **)realloc (p->paths, (p->path_count+1)*sizeof (*next));
  if (!next) { free (copy); return MULIB_METAPP_MEMORY; }
  p->paths = next; p->paths[p->path_count++] = copy; return 0;
}
int
mulib_metapp_include_resolver (mulib_metapp_t *p, mulib_metapp_include_fn fn, void *user)
{
  if (!p) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  p->include = fn; p->include_user = user; return 0;
}
int
mulib_metapp_defined (const mulib_metapp_t *p, const char *name)
{ return p && name && (mulib_metapp_builtin_ (p, name) || mulib_metapp_find_ (p, name, strlen (name))); }
int
mulib_metapp_undefine (mulib_metapp_t *p, const char *name)
{
  size_t i;
  if (!p || !name || !*name || mulib_metapp_builtin_ (p, name)) return MULIB_METAPP_INVALID;
  if (p->busy && !p->callback_mutation) return MULIB_METAPP_BUSY;
  for (i = 0; i < p->macro_count; ++i) if (!strcmp (p->macros[i]->name, name)) {
    if (p->busy) { p->macros[i]->retired = p->retired; p->retired = p->macros[i]; }
    else mulib_metapp_macro_free_ (p->macros[i]);
    memmove (p->macros+i, p->macros+i+1, (--p->macro_count-i)*sizeof (*p->macros)); break;
  }
  return 0;
}
int
mulib_metapp_define (mulib_metapp_t *p, const char *definition, mulib_metapp_error_t *error)
{
  mulib_metapp_tokens_ tokens = {0};
  size_t i;
  int rc;
  if (!p || !definition) return MULIB_METAPP_INVALID;
  if (p->busy && !p->callback_mutation) return MULIB_METAPP_BUSY;
  if (!p->busy) mulib_metapp_reset_ (p);
  rc = mulib_metapp_lex_ (p, "<define>", definition, strlen (definition), &tokens);
  for (i = 0; !rc && i < tokens.n; ++i)
    if (tokens.v[i].kind == MULIB_METAPP_NL_) rc = mulib_metapp_fail_ (p, MULIB_METAPP_SYNTAX, tokens.v+i, "newline in API macro definition");
  if (!rc) rc = mulib_metapp_define_tokens_ (p, tokens.v, tokens.n);
  free (tokens.v); if (error) *error = p->error;
  return rc;
}
int
mulib_metapp_process (mulib_metapp_t *p, const char *name, const char *src,
    size_t n, mulib_strbuf_t *out, mulib_metapp_error_t *error)
{
  int rc;
  if (!p || !name || !*name || (!src && n) || !out) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  mulib_metapp_reset_ (p); p->busy = 1;
  rc = mulib_metapp_source_ (p, name, src, n, 0, 0);
  if (!rc && !mulib_strbuf_append_n (out, p->result.data, p->result.length)) rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "output allocation failed");
  p->busy = 0;
  if (error) *error = p->error;
  return rc;
}
int
mulib_metapp_file (mulib_metapp_t *p, const char *path, mulib_strbuf_t *out, mulib_metapp_error_t *error)
{
  mulib_metapp_source_t source;
  int rc;
  if (!p || !path || !out) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  memset (&source, 0, sizeof (source)); rc = mulib_metapp_read_ (path, p->options.source_bytes, &source);
  if (!rc) { rc = mulib_metapp_process (p, path, source.data, source.length, out, error); source.release (source.user); }
  else { mulib_metapp_reset_ (p); mulib_metapp_fail_ (p, rc, NULL, "cannot read source file"); if (error) *error = p->error; }
  return rc;
}
int
mulib_metapp_plugin (mulib_metapp_t *p, const mulib_metapp_plugin_t *plugin)
{
  char *name;
  size_t i;
  if (!p || !plugin || plugin->abi != MULIB_METAPP_PLUGIN_ABI || plugin->size != sizeof (*plugin)
      || !plugin->name || !*plugin->name || !plugin->handle) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  if (p->plugin_count == 64) return MULIB_METAPP_LIMIT;
  for (i = 0; i < p->plugin_count; ++i) if (!strcmp (p->plugins[i].plugin.name, plugin->name)) return MULIB_METAPP_INVALID;
  name = mulib_metapp_copy_ (plugin->name, strlen (plugin->name)); if (!name) return MULIB_METAPP_MEMORY;
  p->plugins[p->plugin_count].plugin = *plugin;
  p->plugins[p->plugin_count++].plugin.name = name; return 0;
}
#ifdef MULIB_METAPP_NATIVE_
typedef struct mulib_metapp_section_ {
  mulib_elf_section_t elf;
  size_t offset, mapped;
} mulib_metapp_section_;
static uint64_t
mulib_metapp_le64_ (const unsigned char *p)
{ return (uint64_t)mulib_endian_load_le32 (p) | (uint64_t)mulib_endian_load_le32 (p+4) << 32; }
static int
mulib_metapp_executable_ (uintptr_t address, uintptr_t base, const mulib_metapp_section_ *sections, size_t n)
{
  size_t i;
  if (address < base) return 0;
  for (i = 0; i < n; ++i)
    if ((sections[i].elf.flags & 4) && sections[i].mapped && address-base >= sections[i].offset
        && address-base-sections[i].offset < sections[i].elf.size) return 1;
  return 0;
}
#endif
int
mulib_metapp_plugin_link (mulib_metapp_t *p, const void *object, size_t length, mulib_metapp_error_t *error)
{
  int rc = MULIB_METAPP_UNSUPPORTED;
  const char *message = "native plugin linking requires Linux AMD64";
  if (!p || (!object && length)) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  memset (&p->error, 0, sizeof (p->error));
#ifdef MULIB_METAPP_NATIVE_
  {
    mulib_elf_t elf;
    mulib_metapp_section_ *sections = NULL;
    mulib_linker_t *linker = NULL;
    mulib_emitter_t image;
    unsigned char *mapping = NULL;
    size_t i, j, symtab = SIZE_MAX, symbol_count = 0, relocations = 0, mapped = 0, entry = SIZE_MAX;
    long page_long = sysconf (_SC_PAGESIZE);
    size_t page;
    mulib_metapp_plugin_t plugin;
    if (length > p->options.source_bytes || mulib_elf_open (&elf, object, length) || elf.type != 1
        || elf.bits != 64 || !elf.little_endian || elf.machine != 62 || !elf.shnum || elf.shnum > 4096) {
      rc = MULIB_METAPP_PLUGIN; message = "expected bounded ELF64 little-endian AMD64 ET_REL object"; goto finish;
    }
    if (page_long <= 0 || (uintmax_t)page_long > SIZE_MAX) { rc = MULIB_METAPP_IO; message = "cannot determine mapping page size"; goto finish; }
    page = (size_t)page_long; mapped = page;
    sections = (mulib_metapp_section_ *)calloc (elf.shnum, sizeof (*sections));
    if (!sections) { rc = MULIB_METAPP_MEMORY; message = "allocation failed"; goto finish; }
    for (i = 0; i < elf.shnum; ++i) {
      mulib_elf_section_t *s = &sections[i].elf;
      if (mulib_elf_section (&elf, i, s)) { rc = MULIB_METAPP_PLUGIN; message = "invalid ELF section"; goto finish; }
      if (s->type == 2) {
        if (symtab != SIZE_MAX || mulib_elf_symbol_count (&elf, i, &symbol_count) || symbol_count > 65536) {
          rc = MULIB_METAPP_PLUGIN; message = "invalid or multiple ELF symbol tables"; goto finish; }
        symtab = i;
      }
      if (s->type == 4) {
        if (s->entsize != 24 || s->size%24 || s->size/24 > 100000-relocations) { rc = MULIB_METAPP_LIMIT; message = "invalid relocation table or relocation limit"; goto finish; }
        relocations += (size_t)(s->size/24);
      }
      if (!(s->flags & 2) || !s->size) continue;
      if ((s->flags & 0x400) || (s->flags & 5) == 5 || (s->type != 1 && s->type != 8 && s->type != 7) || s->alignment > page) {
        rc = MULIB_METAPP_UNSUPPORTED; message = "unsupported TLS, constructors, section alignment or writable code"; goto finish;
      }
      if (s->size > SIZE_MAX-page+1 || s->size > p->options.arena_bytes) { rc = MULIB_METAPP_LIMIT; message = "plugin image size limit"; goto finish; }
      sections[i].mapped = ((size_t)s->size+page-1)/page*page;
      if (sections[i].mapped > p->options.arena_bytes || mapped > p->options.arena_bytes-sections[i].mapped) { rc = MULIB_METAPP_LIMIT; message = "plugin image size limit"; goto finish; }
      sections[i].offset = mapped; mapped += sections[i].mapped;
    }
    if (symtab == SIZE_MAX || !symbol_count) { rc = MULIB_METAPP_PLUGIN; message = "missing ELF symbol table"; goto finish; }
    {
      int fd = open ("/dev/zero", O_RDWR);
      if (fd < 0) { rc = MULIB_METAPP_IO; message = "cannot open executable mapping backing"; goto finish; }
      mapping = (unsigned char *)mmap (NULL, mapped, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
      (void)close (fd);
      if (mapping == MAP_FAILED) { mapping = NULL; rc = MULIB_METAPP_IO; message = "plugin mapping failed"; goto finish; }
    }
    for (i = 0; i < elf.shnum; ++i) if (sections[i].mapped && sections[i].elf.type != 8)
      memcpy (mapping+sections[i].offset, elf.data+(size_t)sections[i].elf.offset, (size_t)sections[i].elf.size);
    linker = mulib_linker_create (symbol_count, relocations);
    if (!linker) { rc = MULIB_METAPP_MEMORY; message = "linker allocation failed"; goto finish; }
    for (i = 0; i < symbol_count; ++i) {
      mulib_elf_symbol_t symbol;
      const char *name;
      size_t name_length;
      char key[64];
      uint64_t value;
      int absolute = 0;
      if (mulib_elf_symbol (&elf, symtab, i, &symbol) || mulib_elf_symbol_name (&elf, symtab, i, &name, &name_length)) {
        rc = MULIB_METAPP_PLUGIN; message = "invalid symbol"; goto finish;
      }
      (void)name_length;
      if (!symbol.section_index) {
        if (i) { rc = MULIB_METAPP_UNSUPPORTED; message = "plugin imports are unsupported; use the host API table"; goto finish; }
        value = 0; absolute = 1;
      } else if (symbol.section_index == 0xfff1) { value = symbol.value; absolute = 1; }
      else {
        if (symbol.section_index >= elf.shnum) { rc = MULIB_METAPP_UNSUPPORTED; message = "COMMON or extended special symbols unsupported"; goto finish; }
        if (!sections[symbol.section_index].mapped) continue;
        if ((symbol.info & 15) == 10 || (symbol.info & 15) == 6) { rc = MULIB_METAPP_UNSUPPORTED; message = "IFUNC and TLS symbols unsupported"; goto finish; }
        if (symbol.value > sections[symbol.section_index].elf.size || symbol.size > sections[symbol.section_index].elf.size-symbol.value) {
          rc = MULIB_METAPP_PLUGIN; message = "symbol outside section"; goto finish;
        }
        value = sections[symbol.section_index].offset+symbol.value;
        if (!strcmp (name, "mulib_metapp_plugin_v1")) {
          if (entry != SIZE_MAX || (symbol.info & 15) != 1 || symbol.size < sizeof (plugin)) { rc = MULIB_METAPP_PLUGIN; message = "invalid plugin descriptor export"; goto finish; }
          entry = (size_t)value;
        }
      }
      (void)snprintf (key, sizeof (key), "symbol_%zu", i);
      rc = mulib_linker_define (linker, key, value, absolute);
      if (rc) { rc = rc == MULIB_LINKER_ENOMEM ? MULIB_METAPP_MEMORY : MULIB_METAPP_PLUGIN; message = "symbol definition failed"; goto finish; }
    }
    if (entry == SIZE_MAX) { rc = MULIB_METAPP_PLUGIN; message = "missing mulib_metapp_plugin_v1 descriptor"; goto finish; }
    for (i = 0; i < elf.shnum; ++i) {
      const mulib_elf_section_t *s = &sections[i].elf;
      if (s->type != 4 && s->type != 9) continue;
      if (s->info >= elf.shnum) { rc = MULIB_METAPP_PLUGIN; message = "invalid relocation target"; goto finish; }
      if (!sections[s->info].mapped) continue;
      if (s->type != 4 || s->link != symtab) { rc = MULIB_METAPP_UNSUPPORTED; message = "only RELA relocations against the object symbol table are supported"; goto finish; }
      for (j = 0; j < (size_t)(s->size/24); ++j) {
        const unsigned char *record = elf.data+(size_t)s->offset+j*24;
        uint64_t offset = mulib_metapp_le64_ (record), info = mulib_metapp_le64_ (record+8);
        uint64_t raw_addend = mulib_metapp_le64_ (record+16);
        int64_t addend;
        unsigned type = (unsigned)(info & UINT32_MAX), width;
        mulib_linker_kind kind;
        char key[64];
        memcpy (&addend, &raw_addend, sizeof (addend));
        if (!type) continue;
        if (type == 1) { kind = MULIB_LINKER_ABS64; width = 8; }
        else if (type == 2 || type == 4) { kind = MULIB_LINKER_PC32; width = 4; }
        else if (type == 10) { kind = MULIB_LINKER_ABS32; width = 4; }
        else { rc = MULIB_METAPP_UNSUPPORTED; message = "unsupported native relocation (GOT/TLS/32S are not implemented)"; goto finish; }
        if ((info >> 32) >= symbol_count || offset > sections[s->info].elf.size || width > sections[s->info].elf.size-offset) {
          rc = MULIB_METAPP_PLUGIN; message = "relocation outside target section"; goto finish;
        }
        (void)snprintf (key, sizeof (key), "symbol_%llu", (unsigned long long)(info >> 32));
        rc = mulib_linker_relocate (linker, key, sections[s->info].offset+(size_t)offset, kind, addend);
        if (rc) {
          rc = rc == MULIB_LINKER_ENOMEM ? MULIB_METAPP_MEMORY : MULIB_METAPP_PLUGIN; message = "overlapping or invalid relocation"; goto finish;
        }
      }
    }
    (void)mulib_emitter_init (&image, mapping, mapped, MULIB_EMITTER_LITTLE); image.size = mapped;
    if (mulib_linker_apply (linker, &image, (uint64_t)(uintptr_t)mapping, NULL)) {
      rc = MULIB_METAPP_PLUGIN; message = "unresolved or out-of-range plugin relocation"; goto finish;
    }
    memcpy (&plugin, mapping+entry, sizeof (plugin));
    {
      uintptr_t base = (uintptr_t)mapping, name = (uintptr_t)plugin.name, handle = 0, destroy = 0;
      if (sizeof (handle) != sizeof (plugin.handle) || sizeof (destroy) != sizeof (plugin.destroy)) {
        rc = MULIB_METAPP_UNSUPPORTED; message = "unsupported function-pointer ABI"; goto finish;
      }
      memcpy (&handle, &plugin.handle, sizeof (handle)); memcpy (&destroy, &plugin.destroy, sizeof (destroy));
      if (plugin.abi != MULIB_METAPP_PLUGIN_ABI || plugin.size != sizeof (plugin)
          || name < base || name-base >= mapped || !memchr (plugin.name, 0, mapped-(size_t)(name-base))
          || !mulib_metapp_executable_ (handle, base, sections, elf.shnum)
          || (destroy && !mulib_metapp_executable_ (destroy, base, sections, elf.shnum))
          || (plugin.user && ((uintptr_t)plugin.user < base || (uintptr_t)plugin.user-base >= mapped))) {
        rc = MULIB_METAPP_PLUGIN; message = "invalid linked plugin ABI or pointer ranges"; goto finish;
      }
    }
    if (mprotect (mapping, mapped, PROT_READ)) { rc = MULIB_METAPP_IO; message = "cannot protect plugin image"; goto finish; }
    for (i = 0; i < elf.shnum; ++i) if (sections[i].mapped) {
      int protection = PROT_READ | ((sections[i].elf.flags & 1) ? PROT_WRITE : 0) | ((sections[i].elf.flags & 4) ? PROT_EXEC : 0);
      if (mprotect (mapping+sections[i].offset, sections[i].mapped, protection)) { rc = MULIB_METAPP_IO; message = "cannot protect plugin section"; goto finish; }
    }
    rc = mulib_metapp_plugin (p, &plugin);
    if (!rc) {
      p->plugins[p->plugin_count-1].mapping = mapping; p->plugins[p->plugin_count-1].mapped = mapped; mapping = NULL;
    } else message = "plugin registration failed";
finish:
    mulib_linker_destroy (linker); free (sections);
    if (mapping) (void)munmap (mapping, mapped);
  }
#else
  (void)object; (void)length;
#endif
  if (rc) mulib_metapp_fail_ (p, rc, NULL, message);
  if (error) *error = p->error;
  return rc;
}
int
mulib_metapp_plugin_file (mulib_metapp_t *p, const char *path, mulib_metapp_error_t *error)
{
  mulib_metapp_source_t source;
  int rc;
  if (!p || !path) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  memset (&source, 0, sizeof (source)); rc = mulib_metapp_read_ (path, p->options.source_bytes, &source);
  if (!rc) { rc = mulib_metapp_plugin_link (p, source.data, source.length, error); source.release (source.user); }
  else { memset (&p->error, 0, sizeof (p->error)); mulib_metapp_fail_ (p, rc, NULL, "cannot read plugin object"); if (error) *error = p->error; }
  return rc;
}
/* Include indexes retain directed edges, including guarded cycles. Union-find
 * supplies undirected connectivity without allocating a dense adjacency matrix. */
static char *
mulib_metapp_graph_name_ (const mulib_metapp_graph_t *g, const char *name)
{
  size_t n = strlen (name), prefix = !g->custom && name[0] != '/' ? strlen (g->cwd) : 0;
  char *out;
  if (n > SIZE_MAX-2 || prefix > SIZE_MAX-n-2) return NULL;
  out = (char *)malloc (prefix+n+2);
  if (!out) return NULL;
  if (prefix) { memcpy (out, g->cwd, prefix); out[prefix++] = '/'; }
  memcpy (out+prefix, name, n+1);
  if (!g->custom) mulib_metapp_normalize_ (out, out);
  return out;
}
static int
mulib_metapp_graph_intern_ (mulib_metapp_graph_t *g, const char *name, size_t *id)
{
  size_t i;
  char *copy;
  for (i = 0; i < g->count; ++i) if (!strcmp (g->files[i], name)) { *id = i; return 0; }
  if (g->count == g->max_files) return MULIB_METAPP_LIMIT;
  if (g->count == g->capacity) {
    size_t cap = g->capacity ? g->capacity*2 : 16;
    char **next;
    if (cap < g->capacity || cap > g->max_files) cap = g->max_files;
    if (cap > SIZE_MAX/sizeof (*next)) return MULIB_METAPP_LIMIT;
    next = (char **)realloc (g->files, cap*sizeof (*next));
    if (!next) return MULIB_METAPP_MEMORY;
    g->files = next; g->capacity = cap;
  }
  copy = mulib_metapp_copy_ (name, strlen (name));
  if (!copy) return MULIB_METAPP_MEMORY;
  *id = g->count; g->files[g->count++] = copy; return 0;
}
static int
mulib_metapp_graph_edge_compare_ (const void *a, const void *b)
{
  const mulib_metapp_dependency_t *x = (const mulib_metapp_dependency_t *)a, *y = (const mulib_metapp_dependency_t *)b;
  if (x->from != y->from) return x->from < y->from ? -1 : 1;
  return x->to < y->to ? -1 : x->to != y->to;
}
static int
mulib_metapp_graph_record_ (mulib_metapp_t *p, const char *from, const char *to)
{
  mulib_metapp_graph_t *g = p->dependencies;
  mulib_metapp_dependency_t edge;
  size_t lo = 0, hi = g->edge_count;
  int rc = mulib_metapp_graph_intern_ (g, to, &edge.to);
  if (!rc && from) rc = mulib_metapp_graph_intern_ (g, from, &edge.from);
  if (rc) return mulib_metapp_fail_ (p, rc, NULL, "dependency file capacity or allocation failure");
  if (!from) return 0;
  while (lo < hi) {
    size_t mid = lo+(hi-lo)/2;
    int cmp = mulib_metapp_graph_edge_compare_ (&g->edges[mid], &edge);
    if (!cmp) return 0;
    if (cmp < 0) lo = mid+1; else hi = mid;
  }
  if (g->edge_count == g->max_edges) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "dependency edge limit");
  if (g->edge_count == g->edge_capacity) {
    size_t cap = g->edge_capacity ? g->edge_capacity*2 : 16;
    mulib_metapp_dependency_t *next;
    if (cap < g->edge_capacity || cap > g->max_edges) cap = g->max_edges;
    if (cap > SIZE_MAX/sizeof (*next)) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "dependency edge overflow");
    next = (mulib_metapp_dependency_t *)realloc (g->edges, cap*sizeof (*next));
    if (!next) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed");
    g->edges = next; g->edge_capacity = cap;
  }
  memmove (g->edges+lo+1, g->edges+lo, (g->edge_count-lo)*sizeof (*g->edges));
  g->edges[lo] = edge; ++g->edge_count; return 0;
}
typedef struct mulib_metapp_graph_order_ { char *name; size_t old; } mulib_metapp_graph_order_;
static int
mulib_metapp_graph_order_compare_ (const void *a, const void *b)
{ return strcmp (((const mulib_metapp_graph_order_ *)a)->name, ((const mulib_metapp_graph_order_ *)b)->name); }
static int
mulib_metapp_graph_seal_ (mulib_metapp_graph_t *g)
{
  mulib_metapp_graph_order_ *order;
  size_t *map, i;
  mulib_dsu_t sets = {0};
  if (!g->count) return 0;
  if (g->count > SIZE_MAX/sizeof (*order)) return MULIB_METAPP_LIMIT;
  order = (mulib_metapp_graph_order_ *)malloc (g->count*sizeof (*order));
  map = (size_t *)malloc (g->count*sizeof (*map));
  if (!order || !map || !mulib_dsu_init (&sets, g->count)) { free (order); free (map); return MULIB_METAPP_MEMORY; }
  for (i = 0; i < g->count; ++i) { order[i].name = g->files[i]; order[i].old = i; }
  qsort (order, g->count, sizeof (*order), mulib_metapp_graph_order_compare_);
  for (i = 0; i < g->count; ++i) { g->files[i] = order[i].name; map[order[i].old] = i; }
  for (i = 0; i < g->edge_count; ++i) {
    g->edges[i].from = map[g->edges[i].from]; g->edges[i].to = map[g->edges[i].to];
    (void)mulib_dsu_union (&sets, g->edges[i].from, g->edges[i].to);
  }
  if (g->edge_count) qsort (g->edges, g->edge_count, sizeof (*g->edges), mulib_metapp_graph_edge_compare_);
  for (i = 0; i < g->count; ++i) map[i] = mulib_dsu_find (&sets, i);
  g->component = map; free (order); mulib_dsu_free (&sets); return 0;
}
void
mulib_metapp_graph_destroy (mulib_metapp_graph_t *g)
{
  size_t i;
  if (!g) return;
  for (i = 0; i < g->count; ++i) free (g->files[i]);
  free (g->files); free (g->edges); free (g->component); free (g->cwd); free (g);
}
size_t mulib_metapp_graph_count (const mulib_metapp_graph_t *g) { return g ? g->count : 0; }
const char *mulib_metapp_graph_file (const mulib_metapp_graph_t *g, size_t id)
{ return g && id < g->count ? g->files[id] : NULL; }
size_t mulib_metapp_graph_edge_count (const mulib_metapp_graph_t *g) { return g ? g->edge_count : 0; }
const mulib_metapp_dependency_t *mulib_metapp_graph_edge (const mulib_metapp_graph_t *g, size_t id)
{ return g && id < g->edge_count ? g->edges+id : NULL; }
static mulib_metapp_macro_ *
mulib_metapp_macro_copy_ (const mulib_metapp_macro_ *m)
{
  mulib_metapp_macro_ *n = (mulib_metapp_macro_ *)calloc (1, sizeof (*n));
  size_t i;
  if (!n) return NULL;
  n->name = mulib_metapp_copy_ (m->name, strlen (m->name));
  n->function = m->function; n->variadic = m->variadic; n->id = m->id;
  if (!n->name) goto fail;
  if (m->count) {
    n->params = (char **)calloc (m->count, sizeof (*n->params));
    if (!n->params) goto fail;
    for (i = 0; i < m->count; ++i) {
      n->params[n->count++] = mulib_metapp_copy_ (m->params[i], strlen (m->params[i]));
      if (!n->params[i]) goto fail;
    }
  }
  if (m->body.n) {
    n->body.v = (mulib_metapp_token_ *)calloc (m->body.n, sizeof (*n->body.v));
    if (!n->body.v) goto fail;
    n->body.cap = m->body.n;
    for (i = 0; i < m->body.n; ++i) {
      n->body.v[i] = m->body.v[i];
      n->body.v[n->body.n++].text = mulib_metapp_copy_ (m->body.v[i].text, m->body.v[i].length);
      if (!n->body.v[i].text) goto fail;
    }
  }
  return n;
fail:
  mulib_metapp_macro_free_ (n); return NULL;
}
static mulib_metapp_t *
mulib_metapp_dependency_context_ (const mulib_metapp_t *p, mulib_metapp_graph_t *g)
{
  mulib_metapp_options_t options = p->options;
  mulib_metapp_t *q;
  size_t i;
  options.date = p->date; options.time = p->time;
  q = mulib_metapp_create (&options);
  if (!q) return NULL;
  q->include = p->include; q->include_user = p->include_user; q->next_id = p->next_id;
  for (i = 0; i < p->path_count; ++i) if (mulib_metapp_include_path (q, p->paths[i])) goto fail;
  if (p->macro_count) {
    q->macros = (mulib_metapp_macro_ **)calloc (p->macro_count, sizeof (*q->macros));
    if (!q->macros) goto fail;
    for (i = 0; i < p->macro_count; ++i) {
      q->macros[q->macro_count++] = mulib_metapp_macro_copy_ (p->macros[i]);
      if (!q->macros[i]) goto fail;
    }
  }
  for (i = 0; i < p->plugin_count; ++i) {
    mulib_metapp_plugin_t plugin = p->plugins[i].plugin;
    plugin.destroy = NULL;
    if (mulib_metapp_plugin (q, &plugin)) goto fail;
  }
  q->dependencies = g; return q;
fail:
  mulib_metapp_destroy (q); return NULL;
}
static mulib_metapp_graph_t *
mulib_metapp_graph_create_ (mulib_metapp_t *p, const mulib_metapp_dependency_options_t *options)
{
  mulib_metapp_graph_t *g = (mulib_metapp_graph_t *)calloc (1, sizeof (*g));
  if (!g) { mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed"); return NULL; }
  g->max_files = options && options->max_files ? options->max_files : 4096;
  g->max_edges = options && options->max_edges ? options->max_edges : 32768;
  g->custom = p->include != NULL;
  if (g->max_files > SIZE_MAX/sizeof (mulib_metapp_graph_order_) || g->max_edges > SIZE_MAX/sizeof (*g->edges)) {
    mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "dependency capacity overflow"); goto fail;
  }
#if defined(__unix__) || defined(__APPLE__)
  if (!g->custom) {
    size_t capacity = 256;
    for (;;) {
      g->cwd = (char *)malloc (capacity);
      if (!g->cwd) { mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed"); goto fail; }
      if (getcwd (g->cwd, capacity)) break;
      free (g->cwd); g->cwd = NULL;
      if (errno != ERANGE || capacity == 1024*1024) {
        mulib_metapp_fail_ (p, MULIB_METAPP_IO, NULL, "cannot resolve dependency working directory"); goto fail;
      }
      capacity *= 2;
    }
  }
#endif
  if (!g->cwd) g->cwd = mulib_metapp_copy_ ("", 0);
  if (!g->cwd) { mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed"); goto fail; }
  return g;
fail:
  mulib_metapp_graph_destroy (g); return NULL;
}
static int
mulib_metapp_dependency_root_ (mulib_metapp_t *p, mulib_metapp_graph_t *g, const char *root)
{
  mulib_metapp_t *q = mulib_metapp_dependency_context_ (p, g);
  mulib_metapp_source_t source;
  int rc;
  if (!q) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "dependency context allocation failed");
  memset (&source, 0, sizeof (source)); q->busy = 1;
  rc = q->include ? mulib_metapp_open_ (q, NULL, root, 0, &source) : mulib_metapp_read_ (root, q->options.source_bytes, &source);
  if (!rc) {
    rc = mulib_metapp_source_ (q, source.name, source.data, source.length, 0, 0);
    if (source.release) source.release (source.user);
  } else {
    q->physical_file = root; mulib_metapp_fail_ (q, rc, NULL, "cannot read dependency root"); q->physical_file = NULL;
  }
  if (rc) p->error = q->error;
  q->busy = 0; mulib_metapp_destroy (q); return rc;
}
static int mulib_metapp_string_compare_ (const void *a, const void *b)
{ return strcmp (*(const char *const *)a, *(const char *const *)b); }
static int
mulib_metapp_dependency_roots_ (mulib_metapp_t *p, mulib_metapp_graph_t *g,
    const char *const *roots, size_t count, int unvisited)
{
  char **names;
  size_t i, j;
  int rc = 0;
  if (count > g->max_files || count > SIZE_MAX/sizeof (*names)) return mulib_metapp_fail_ (p, MULIB_METAPP_LIMIT, NULL, "dependency root limit");
  if (!count) return 0;
  names = (char **)calloc (count, sizeof (*names));
  if (!names) return mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed");
  for (i = 0; i < count; ++i) {
    if (!roots[i] || !*roots[i]) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_INVALID, NULL, "empty dependency root"); break; }
    names[i] = mulib_metapp_graph_name_ (g, roots[i]);
    if (!names[i]) { rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed"); break; }
  }
  if (!rc) {
    qsort (names, count, sizeof (*names), mulib_metapp_string_compare_);
    for (i = 0; i < count && !rc; ++i) {
      if (i && !strcmp (names[i], names[i-1])) continue;
      if (unvisited) {
        for (j = 0; j < g->count; ++j) if (!strcmp (g->files[j], names[i])) break;
        if (j < g->count) continue;
      }
      rc = mulib_metapp_dependency_root_ (p, g, names[i]);
    }
  }
  for (i = 0; i < count; ++i) free (names[i]);
  free (names); return rc;
}
int
mulib_metapp_dependencies (mulib_metapp_t *p, const char *const *roots, size_t count,
    const mulib_metapp_dependency_options_t *options, mulib_metapp_graph_t **out, mulib_metapp_error_t *error)
{
  mulib_metapp_graph_t *g;
  int rc;
  if (!p || (!roots && count) || !out || *out) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  memset (&p->error, 0, sizeof (p->error)); p->busy = 1;
  g = mulib_metapp_graph_create_ (p, options);
  rc = g ? mulib_metapp_dependency_roots_ (p, g, roots, count, 0) : p->error.code;
  if (!rc) rc = mulib_metapp_graph_seal_ (g);
  if (rc) { mulib_metapp_fail_ (p, rc, NULL, "dependency index allocation failed"); mulib_metapp_graph_destroy (g); }
  else *out = g;
  p->busy = 0; if (error) *error = p->error; return rc;
}
#if defined(__unix__) || defined(__APPLE__)
typedef struct mulib_metapp_tree_ {
  char **sources, **headers;
  size_t source_count, header_count, entries, max_entries;
  mulib_metapp_t *pp;
  mulib_metapp_graph_t *graph;
} mulib_metapp_tree_;
static int
mulib_metapp_tree_walk_ (mulib_metapp_tree_ *t, const char *path, size_t depth)
{
  struct stat info;
  int rc = 0;
  if (depth > 128 || t->entries++ == t->max_entries) return mulib_metapp_fail_ (t->pp, MULIB_METAPP_LIMIT, NULL, "dependency tree traversal limit");
  if (lstat (path, &info)) return mulib_metapp_fail_ (t->pp, MULIB_METAPP_IO, NULL, "cannot stat dependency tree entry");
  if (!depth && !S_ISDIR (info.st_mode)) return mulib_metapp_fail_ (t->pp, MULIB_METAPP_INVALID, NULL, "dependency tree root must be a directory");
  if (S_ISREG (info.st_mode)) {
    const char *extension = strrchr (path, '.');
    int source;
    char ***list, **next, *name;
    size_t *count;
    if (!extension) return 0;
    source = !strcmp (extension, ".c") || !strcmp (extension, ".C") || !strcmp (extension, ".cc")
      || !strcmp (extension, ".cpp") || !strcmp (extension, ".cxx") || !strcmp (extension, ".c++");
    if (!source && strcmp (extension, ".h") && strcmp (extension, ".H") && strcmp (extension, ".hh")
        && strcmp (extension, ".hpp") && strcmp (extension, ".hxx") && strcmp (extension, ".inc") && strcmp (extension, ".inl")) return 0;
    if (t->source_count+t->header_count == t->graph->max_files) return mulib_metapp_fail_ (t->pp, MULIB_METAPP_LIMIT, NULL, "dependency tree file limit");
    list = source ? &t->sources : &t->headers; count = source ? &t->source_count : &t->header_count;
    name = mulib_metapp_copy_ (path, strlen (path));
    if (!name) return mulib_metapp_fail_ (t->pp, MULIB_METAPP_MEMORY, NULL, "allocation failed");
    next = (char **)realloc (*list, (*count+1)*sizeof (*next));
    if (!next) { free (name); return mulib_metapp_fail_ (t->pp, MULIB_METAPP_MEMORY, NULL, "allocation failed"); }
    *list = next; (*list)[(*count)++] = name;
  } else if (S_ISDIR (info.st_mode)) {
    DIR *dir = opendir (path);
    struct dirent *entry;
    if (!dir) return mulib_metapp_fail_ (t->pp, MULIB_METAPP_IO, NULL, "cannot open dependency directory");
    while (!rc) {
      mulib_strbuf_t child = {0};
      errno = 0; entry = readdir (dir);
      if (!entry) { if (errno) rc = mulib_metapp_fail_ (t->pp, MULIB_METAPP_IO, NULL, "cannot read dependency directory"); break; }
      if (entry->d_name[0] == '.') {
        if (strcmp (entry->d_name, ".") && strcmp (entry->d_name, "..") && t->entries++ == t->max_entries)
          rc = mulib_metapp_fail_ (t->pp, MULIB_METAPP_LIMIT, NULL, "dependency tree traversal limit");
        continue;
      }
      if (!mulib_strbuf_append (&child, path) || !mulib_strbuf_putc (&child, '/') || !mulib_strbuf_append (&child, entry->d_name))
        rc = mulib_metapp_fail_ (t->pp, MULIB_METAPP_MEMORY, NULL, "allocation failed");
      if (!rc) rc = mulib_metapp_tree_walk_ (t, child.data, depth+1);
      mulib_strbuf_free (&child);
    }
    if (closedir (dir) && !rc) rc = mulib_metapp_fail_ (t->pp, MULIB_METAPP_IO, NULL, "cannot close dependency directory");
  }
  return rc;
}
#endif
int
mulib_metapp_dependencies_tree (mulib_metapp_t *p, const char *directory,
    const mulib_metapp_dependency_options_t *options, mulib_metapp_graph_t **out, mulib_metapp_error_t *error)
{
  int rc = MULIB_METAPP_UNSUPPORTED;
  if (!p || !directory || !*directory || !out || *out) return MULIB_METAPP_INVALID;
  if (p->busy) return MULIB_METAPP_BUSY;
  memset (&p->error, 0, sizeof (p->error)); p->busy = 1;
  if (p->include) rc = mulib_metapp_fail_ (p, MULIB_METAPP_INVALID, NULL, "tree discovery requires native includes; use explicit resolver roots");
#if defined(__unix__) || defined(__APPLE__)
  else {
    mulib_metapp_tree_ tree;
    char *root = NULL;
    size_t i;
    memset (&tree, 0, sizeof (tree)); tree.pp = p;
    tree.max_entries = options && options->max_entries ? options->max_entries : 100000;
    tree.graph = mulib_metapp_graph_create_ (p, options);
    rc = tree.graph ? 0 : p->error.code;
    if (!rc) { root = mulib_metapp_graph_name_ (tree.graph, directory); if (!root) rc = mulib_metapp_fail_ (p, MULIB_METAPP_MEMORY, NULL, "allocation failed"); }
    if (!rc) rc = mulib_metapp_tree_walk_ (&tree, root, 0);
    if (!rc) rc = mulib_metapp_dependency_roots_ (p, tree.graph, (const char *const *)tree.sources, tree.source_count, 0);
    if (!rc) rc = mulib_metapp_dependency_roots_ (p, tree.graph, (const char *const *)tree.headers, tree.header_count, 1);
    if (!rc) rc = mulib_metapp_graph_seal_ (tree.graph);
    if (!rc) *out = tree.graph; else mulib_metapp_graph_destroy (tree.graph);
    for (i = 0; i < tree.source_count; ++i) free (tree.sources[i]);
    for (i = 0; i < tree.header_count; ++i) free (tree.headers[i]);
    free (tree.sources); free (tree.headers); free (root);
  }
#endif
  if (rc) mulib_metapp_fail_ (p, rc, NULL, "dependency tree discovery unavailable or allocation failed");
  p->busy = 0; if (error) *error = p->error; return rc;
}
int
mulib_metapp_graph_component (const mulib_metapp_graph_t *g, const char *file, mulib_metapp_graph_t **out)
{
  mulib_metapp_graph_t *part;
  size_t id, i, *map;
  char *name;
  int rc = 0;
  if (!g || !file || !*file || !out || *out) return MULIB_METAPP_INVALID;
  name = mulib_metapp_graph_name_ (g, file);
  if (!name) return MULIB_METAPP_MEMORY;
  for (id = 0; id < g->count; ++id) if (!strcmp (g->files[id], name)) break;
  free (name);
  if (id == g->count) return MULIB_METAPP_NOTFOUND;
  part = (mulib_metapp_graph_t *)calloc (1, sizeof (*part));
  if (!part) return MULIB_METAPP_MEMORY;
  part->max_files = g->count; part->max_edges = g->edge_count; part->custom = g->custom;
  part->cwd = mulib_metapp_copy_ (g->cwd, strlen (g->cwd));
  map = (size_t *)malloc (g->count*sizeof (*map));
  if (!part->cwd || !map) { free (map); mulib_metapp_graph_destroy (part); return MULIB_METAPP_MEMORY; }
  for (i = 0; i < g->count; ++i) {
    map[i] = SIZE_MAX;
    if (g->component[i] == g->component[id]) { rc = mulib_metapp_graph_intern_ (part, g->files[i], map+i); if (rc) break; }
  }
  if (!rc && g->edge_count) {
    part->edges = (mulib_metapp_dependency_t *)malloc (g->edge_count*sizeof (*part->edges));
    if (!part->edges) rc = MULIB_METAPP_MEMORY;
    for (i = 0; !rc && i < g->edge_count; ++i) if (map[g->edges[i].from] != SIZE_MAX) {
      part->edges[part->edge_count].from = map[g->edges[i].from];
      part->edges[part->edge_count++].to = map[g->edges[i].to];
    }
  }
  if (!rc) { part->component = (size_t *)calloc (part->count, sizeof (*part->component)); if (!part->component) rc = MULIB_METAPP_MEMORY; }
  free (map);
  if (rc) mulib_metapp_graph_destroy (part); else *out = part;
  return rc;
}
static int
mulib_metapp_dot_string_ (mulib_strbuf_t *out, const char *s)
{
  if (!mulib_strbuf_putc (out, '"')) return 0;
  for (; *s; ++s) {
    if (*s == '\n' || *s == '\r') { if (!mulib_strbuf_append (out, *s == '\n' ? "\\n" : "\\r")) return 0; }
    else {
      if ((*s == '\\' || *s == '"') && !mulib_strbuf_putc (out, '\\')) return 0;
      if (!mulib_strbuf_putc (out, *s)) return 0;
    }
  }
  return mulib_strbuf_putc (out, '"');
}
int
mulib_metapp_graph_dot (const mulib_metapp_graph_t *g, mulib_strbuf_t *out)
{
  mulib_strbuf_t text = {0};
  size_t i;
  char line[96];
  int ok;
  if (!g || !out) return MULIB_METAPP_INVALID;
  ok = mulib_strbuf_append (&text, "digraph metapp {\n");
  for (i = 0; ok && i < g->count; ++i) {
    (void)snprintf (line, sizeof (line), "  n%zu [label=", i);
    ok = mulib_strbuf_append (&text, line) && mulib_metapp_dot_string_ (&text, g->files[i]) && mulib_strbuf_append (&text, "];\n");
  }
  for (i = 0; ok && i < g->edge_count; ++i) {
    (void)snprintf (line, sizeof (line), "  n%zu -> n%zu;\n", g->edges[i].from, g->edges[i].to);
    ok = mulib_strbuf_append (&text, line);
  }
  if (ok) ok = mulib_strbuf_append (&text, "}\n") && mulib_strbuf_append_n (out, text.data, text.length);
  mulib_strbuf_free (&text); return ok ? 0 : MULIB_METAPP_MEMORY;
}
#endif /* MULIB_IMPL_METAPP */
