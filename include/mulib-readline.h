/** @file mulib-readline.h
 * @brief Bounded UTF-8 line editing, highlighting, completion and declarative directives.
 * Define MULIB_IMPL_READLINE plus JSON, TERMKEY, TERMUTILS and their dependencies.
 * Profiles are versioned JSON in .mlsyn/.mlac/.mldir files. See the manual. */
#ifndef MULIB_READLINE_H
#define MULIB_READLINE_H
#include "mulib-json.h"
#include "mulib-termkey.h"
#include "mulib-termutils.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_readline mulib_readline_t;
typedef enum mulib_readline_result
{
  MULIB_READLINE_OK = 0, MULIB_READLINE_LINE = 1, MULIB_READLINE_EOF = 2,
  MULIB_READLINE_INTERRUPT = 3, MULIB_READLINE_DIRECTIVE = 4,
  MULIB_READLINE_INVALID = -1, MULIB_READLINE_MEMORY = -2,
  MULIB_READLINE_LIMIT = -3, MULIB_READLINE_IO = -4,
  MULIB_READLINE_NOT_FOUND = -5, MULIB_READLINE_BUSY = -6,
  MULIB_READLINE_UNSUPPORTED = -7
} mulib_readline_result_t;
typedef enum mulib_readline_style
{
  MULIB_READLINE_PLAIN, MULIB_READLINE_KEYWORD, MULIB_READLINE_STRING,
  MULIB_READLINE_COMMENT, MULIB_READLINE_NUMBER, MULIB_READLINE_TYPE,
  MULIB_READLINE_FUNCTION, MULIB_READLINE_DIRECTIVE_STYLE
} mulib_readline_style_t;
typedef enum mulib_readline_rule_kind
{ MULIB_READLINE_WORD, MULIB_READLINE_PREFIX, MULIB_READLINE_SPAN, MULIB_READLINE_NUMERIC } mulib_readline_rule_kind_t;
typedef enum mulib_readline_profile_kind
{ MULIB_READLINE_SYNTAX, MULIB_READLINE_COMPLETIONS, MULIB_READLINE_DIRECTIVES } mulib_readline_profile_kind_t;
typedef struct mulib_readline_limits
{
  size_t line_bytes, history_entries, history_bytes, config_entries, config_bytes;
} mulib_readline_limits_t;
/** @brief Zero fields select 64 KiB line, 256 history lines/1 MiB, 4096 config
 * entries/1 MiB. All returned line strings are malloc-owned. */
mulib_readline_t *mulib_readline_create (const mulib_readline_limits_t *limits);
/** @brief Release an idle editor; callbacks may not destroy/reenter it. */
void mulib_readline_destroy (mulib_readline_t *editor);
/** @brief Replace the edit buffer atomically; UTF-8, no control bytes. */
int mulib_readline_set (mulib_readline_t *editor, const char *text, size_t length);
/** @brief Borrow the NUL-terminated line until mutation; cursor is a byte offset. */
const char *mulib_readline_text (const mulib_readline_t *editor, size_t *length, size_t *cursor);
/** @brief Process one decoded key. LINE/EOF/INTERRUPT are terminal outcomes;
 * editing and completion return OK. Paste text replaces tabs/newlines by spaces. */
int mulib_readline_edit (mulib_readline_t *editor, const mulib_termkey_event_t *event);
/** @brief Add an ordered lexical rule. WORD matches whole identifier; PREFIX
 * consumes to newline; SPAN uses optional one-byte escape. Numeric ignores start. */
int mulib_readline_rule (mulib_readline_t *, mulib_readline_rule_kind_t,
    mulib_readline_style_t, const char *start, const char *end, char escape);
/** @brief Add a completion, copying both fields. Duplicate text is a no-op. */
int mulib_readline_completion (mulib_readline_t *, const char *text, const char *description);
/** @brief Add a named :directive with exactly one action or expansion. Expansion
 * substitutes $* with argument text; actions are dispatched by the application. */
int mulib_readline_directive (mulib_readline_t *, const char *name,
    const char *action, const char *expansion, const char *help);
typedef int (*mulib_readline_span_fn) (void *, size_t offset, size_t length, mulib_readline_style_t);
typedef int (*mulib_readline_complete_fn) (void *, const char *text, const char *description);
/** @brief Visit contiguous styled spans; callback nonzero stops enumeration. */
int mulib_readline_highlight (mulib_readline_t *, const char *, size_t, mulib_readline_span_fn, void *);
/** @brief Visit prefix matches at cursor; replacement_start identifies the word.
 * Callback strings borrow editor storage. Callback nonzero stops enumeration. */
int mulib_readline_complete (mulib_readline_t *, mulib_readline_complete_fn, void *, size_t *replacement_start);
/** @brief Apply a zero-based matching completion atomically. */
int mulib_readline_accept (mulib_readline_t *, size_t index);
/** @brief Decode :name arguments. action/arguments borrow editor/line storage;
 * expansion is malloc-owned on success. Outputs change only for a known directive. */
int mulib_readline_dispatch (mulib_readline_t *, const char *line,
    const char **action, const char **arguments, char **expansion);
/** @brief Transactionally append profile definitions. Error line is a 1-based
 * JSON syntax line, or zero for schema/quota errors. */
int mulib_readline_load_memory (mulib_readline_t *, mulib_readline_profile_kind_t,
    const char *json, size_t length, size_t *error_line);
/** @brief Load a profile, inferring its kind from the exact filename extension. */
int mulib_readline_load (mulib_readline_t *, const char *path, size_t *error_line);
/** @brief Copy a line into bounded history, evicting oldest entries; consecutive
 * duplicates and empty lines are ignored. */
int mulib_readline_history_add (mulib_readline_t *, const char *line);
/** @brief Render one horizontally clipped ANSI line into malloc-owned storage.
 * Prompt must contain printable ASCII; columns >= 8. Controls never reach output. */
int mulib_readline_render (mulib_readline_t *, const char *prompt, unsigned columns, char **output);
/** @brief Read one line from descriptors. POSIX TTYs use raw editing with restored
 * attributes on return; pipes use bounded plain lines. *line must be NULL.
 * The editor is externally serialized; this call does not dispatch directives. */
int mulib_readline_read (mulib_readline_t *, int input_fd, int output_fd,
    const char *prompt, char **line);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_READLINE) && !defined(MULIB_READLINE_IMPLEMENTED)
#define MULIB_READLINE_IMPLEMENTED
#include "mulib-strbuf.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <poll.h>
#endif
typedef struct mulib_readline_rule_
{ mulib_readline_rule_kind_t kind; mulib_readline_style_t style; char *start, *end; char escape; } mulib_readline_rule_;
typedef struct mulib_readline_item_
{ char *name, *value, *help; int expansion; } mulib_readline_item_;
struct mulib_readline
{
  mulib_readline_limits_t limits;
  char *line, *draft;
  size_t length, cursor, history_at, history_count, history_bytes, config_bytes;
  char **history;
  mulib_readline_rule_ *rules;
  mulib_readline_item_ *completions, *directives;
  size_t rule_count, completion_count, directive_count;
  unsigned char paste_pending[4];
  size_t paste_length;
  int busy;
};
static char *mulib_readline_copy_ (const char *p, size_t n)
{
  char *s;
  if ((!p && n) || n == SIZE_MAX) return NULL;
  s = (char *)malloc (n + 1);
  if (s) { if (n) memcpy (s, p, n); s[n] = 0; }
  return s;
}
static int mulib_readline_clean_ (const char *p, size_t n)
{
  size_t i = 0, used;
  uint32_t cp;
  if ((!p && n) || !mulib_utf8_validate ((const unsigned char *)p, n)) return 0;
  while (i < n)
    {
      if (!mulib_utf8_decode ((const unsigned char *)p + i, n - i, &cp, &used)
          || cp < 32 || (cp >= 127 && cp <= 159)) return 0;
      i += used;
    }
  return 1;
}
mulib_readline_t *mulib_readline_create (const mulib_readline_limits_t *l)
{
  mulib_readline_t *r = (mulib_readline_t *)calloc (1, sizeof (*r));
  if (!r) return NULL;
  if (l) r->limits = *l;
  if (!r->limits.line_bytes) r->limits.line_bytes = 65536;
  if (!r->limits.history_entries) r->limits.history_entries = 256;
  if (!r->limits.history_bytes) r->limits.history_bytes = 1048576;
  if (!r->limits.config_entries) r->limits.config_entries = 4096;
  if (!r->limits.config_bytes) r->limits.config_bytes = 1048576;
  if (r->limits.line_bytes == SIZE_MAX || r->limits.history_entries > SIZE_MAX / sizeof (char *)
      || r->limits.config_entries > SIZE_MAX / sizeof (mulib_readline_rule_)
      || r->limits.config_entries > SIZE_MAX / sizeof (mulib_readline_item_)) { free (r); return NULL; }
  r->line = mulib_readline_copy_ ("", 0);
  if (!r->line) { free (r); return NULL; }
  return r;
}
static void mulib_readline_trim_ (mulib_readline_t *r, size_t rules, size_t ac, size_t dirs)
{
  while (r->rule_count > rules)
    { mulib_readline_rule_ *v = &r->rules[--r->rule_count]; free (v->start); free (v->end); }
  while (r->completion_count > ac)
    { mulib_readline_item_ *v = &r->completions[--r->completion_count]; free (v->name); free (v->help); }
  while (r->directive_count > dirs)
    { mulib_readline_item_ *v = &r->directives[--r->directive_count]; free (v->name); free (v->value); free (v->help); }
}
void mulib_readline_destroy (mulib_readline_t *r)
{
  size_t i;
  if (!r || r->busy) return;
  mulib_readline_trim_ (r, 0, 0, 0);
  free (r->rules); free (r->completions); free (r->directives);
  for (i = 0; i < r->history_count; ++i) free (r->history[i]);
  free (r->history); free (r->line); free (r->draft); free (r);
}
int mulib_readline_set (mulib_readline_t *r, const char *s, size_t n)
{
  char *copy;
  if (!r || !mulib_readline_clean_ (s, n)) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  if (n > r->limits.line_bytes) return MULIB_READLINE_LIMIT;
  copy = mulib_readline_copy_ (s, n);
  if (!copy) return MULIB_READLINE_MEMORY;
  free (r->line); r->line = copy; r->length = r->cursor = n;
  r->paste_length = 0;
  return 0;
}
const char *mulib_readline_text (const mulib_readline_t *r, size_t *n, size_t *cursor)
{
  if (!r) return NULL;
  if (n) *n = r->length;
  if (cursor) *cursor = r->cursor;
  return r->line;
}
static int mulib_readline_quota_ (mulib_readline_t *r, size_t bytes)
{
  if (r->busy) return MULIB_READLINE_BUSY;
  if (r->rule_count + r->completion_count + r->directive_count >= r->limits.config_entries
      || bytes > r->limits.config_bytes - r->config_bytes) return MULIB_READLINE_LIMIT;
  return 0;
}
int mulib_readline_rule (mulib_readline_t *r, mulib_readline_rule_kind_t kind,
    mulib_readline_style_t style, const char *start, const char *end, char escape)
{
  mulib_readline_rule_ v, *next;
  size_t a, b;
  int rc;
  if (!r || kind < 0 || kind > MULIB_READLINE_NUMERIC || style < 0 || style > MULIB_READLINE_DIRECTIVE_STYLE)
    return MULIB_READLINE_INVALID;
  if (!start) start = "";
  if (!end) end = "";
  a = strlen (start); b = strlen (end);
  if ((kind != MULIB_READLINE_NUMERIC && !a) || (kind == MULIB_READLINE_SPAN && !b)
      || !mulib_readline_clean_ (start, a) || !mulib_readline_clean_ (end, b)
      || a > 256 || b > 256 || (escape && ((unsigned char)escape < 32 || (unsigned char)escape > 126)))
    return MULIB_READLINE_INVALID;
  rc = mulib_readline_quota_ (r, a + b + 2); if (rc) return rc;
  v.start = mulib_readline_copy_ (start, a); v.end = mulib_readline_copy_ (end, b);
  if (!v.start || !v.end) { free (v.start); free (v.end); return MULIB_READLINE_MEMORY; }
  next = (mulib_readline_rule_ *)realloc (r->rules, (r->rule_count + 1) * sizeof (*next));
  if (!next) { free (v.start); free (v.end); return MULIB_READLINE_MEMORY; }
  v.kind = kind; v.style = style; v.escape = escape;
  r->rules = next; r->rules[r->rule_count++] = v; r->config_bytes += a + b + 2;
  return 0;
}
int mulib_readline_completion (mulib_readline_t *r, const char *text, const char *description)
{
  mulib_readline_item_ v, *next;
  size_t i, n, h;
  int rc;
  if (!r || !text || !*text) return MULIB_READLINE_INVALID;
  if (!description) description = "";
  n = strlen (text); h = strlen (description);
  if (!mulib_readline_clean_ (text, n) || !mulib_readline_clean_ (description, h)) return MULIB_READLINE_INVALID;
  if (n > r->limits.config_bytes || h > r->limits.config_bytes - n
      || r->limits.config_bytes - n - h < 2) return MULIB_READLINE_LIMIT;
  if (r->busy) return MULIB_READLINE_BUSY;
  for (i = 0; i < r->completion_count; ++i) if (!strcmp (r->completions[i].name, text)) return 0;
  rc = mulib_readline_quota_ (r, n + h + 2); if (rc) return rc;
  memset (&v, 0, sizeof (v)); v.name = mulib_readline_copy_ (text, n); v.help = mulib_readline_copy_ (description, h);
  if (!v.name || !v.help) { free (v.name); free (v.help); return MULIB_READLINE_MEMORY; }
  next = (mulib_readline_item_ *)realloc (r->completions, (r->completion_count + 1) * sizeof (*next));
  if (!next) { free (v.name); free (v.help); return MULIB_READLINE_MEMORY; }
  r->completions = next; next[r->completion_count++] = v; r->config_bytes += n + h + 2;
  return 0;
}
static int mulib_readline_word_ (unsigned char c)
{ return c >= 128 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
int mulib_readline_directive (mulib_readline_t *r, const char *name,
    const char *action, const char *expansion, const char *help)
{
  mulib_readline_item_ v, *next;
  size_t i, n, a, h;
  int rc;
  if (!r || !name || !*name || (!!action == !!expansion)) return MULIB_READLINE_INVALID;
  if (!help) help = "";
  n = strlen (name); a = strlen (action ? action : expansion); h = strlen (help);
  if (!mulib_readline_clean_ (name, n)) return MULIB_READLINE_INVALID;
  for (i = 0; i < n; ++i) if (!mulib_readline_word_ ((unsigned char)name[i]) && name[i] != '-') return MULIB_READLINE_INVALID;
  if (!a || !mulib_readline_clean_ (action ? action : expansion, a) || !mulib_readline_clean_ (help, h)) return MULIB_READLINE_INVALID;
  if (n > r->limits.config_bytes || a > r->limits.config_bytes - n
      || h > r->limits.config_bytes - n - a || r->limits.config_bytes - n - a - h < 3) return MULIB_READLINE_LIMIT;
  rc = mulib_readline_quota_ (r, n + a + h + 3); if (rc) return rc;
  for (i = 0; i < r->directive_count; ++i) if (!strcmp (r->directives[i].name, name)) return MULIB_READLINE_INVALID;
  memset (&v, 0, sizeof (v)); v.name = mulib_readline_copy_ (name, n);
  v.value = mulib_readline_copy_ (action ? action : expansion, a); v.help = mulib_readline_copy_ (help, h);
  if (!v.name || !v.value || !v.help) { free (v.name); free (v.value); free (v.help); return MULIB_READLINE_MEMORY; }
  next = (mulib_readline_item_ *)realloc (r->directives, (r->directive_count + 1) * sizeof (*next));
  if (!next) { free (v.name); free (v.value); free (v.help); return MULIB_READLINE_MEMORY; }
  v.expansion = expansion != NULL; r->directives = next; next[r->directive_count++] = v;
  r->config_bytes += n + a + h + 3; return 0;
}
static size_t mulib_readline_match_ (const char *s, size_t n, size_t at,
                                    const mulib_readline_rule_ *v)
{
  size_t len = strlen (v->start), end = strlen (v->end), p;
  if (v->kind == MULIB_READLINE_NUMERIC)
    {
      if (s[at] < '0' || s[at] > '9' || (at && mulib_readline_word_ ((unsigned char)s[at - 1]))) return 0;
      for (p = at + 1; p < n && (mulib_readline_word_ ((unsigned char)s[p]) || s[p] == '.'); ++p) {}
      return p - at;
    }
  if (len > n - at || memcmp (s + at, v->start, len)) return 0;
  if (v->kind == MULIB_READLINE_WORD)
    return (at && mulib_readline_word_ ((unsigned char)s[at - 1]))
        || (at + len < n && mulib_readline_word_ ((unsigned char)s[at + len])) ? 0 : len;
  if (v->kind == MULIB_READLINE_PREFIX) return n - at;
  p = at + len;
  while (p < n)
    {
      if (v->escape && s[p] == v->escape) { p += n - p > 1 ? 2 : 1; continue; }
      if (end <= n - p && !memcmp (s + p, v->end, end)) return p + end - at;
      ++p;
    }
  return n - at;
}
int mulib_readline_highlight (mulib_readline_t *r, const char *s, size_t n, mulib_readline_span_fn fn, void *user)
{
  size_t at = 0, first = 0, i;
  mulib_readline_style_t previous = MULIB_READLINE_PLAIN;
  int rc = 0;
  if (!r || !fn || !mulib_readline_clean_ (s, n)) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  if (n > r->limits.line_bytes) return MULIB_READLINE_LIMIT;
  r->busy = 1;
  while (at < n && !rc)
    {
      size_t length = 0;
      mulib_readline_style_t style = MULIB_READLINE_PLAIN;
      for (i = 0; i < r->rule_count; ++i)
        if ((length = mulib_readline_match_ (s, n, at, &r->rules[i])) != 0) { style = r->rules[i].style; break; }
      if (!length) { uint32_t cp; (void)mulib_utf8_decode ((const unsigned char *)s + at, n - at, &cp, &length); }
      if (style != previous)
        {
          if (at > first) rc = fn (user, first, at - first, previous);
          first = at; previous = style;
        }
      at += length;
    }
  if (!rc && n > first) rc = fn (user, first, n - first, previous);
  r->busy = 0; return rc;
}
static size_t mulib_readline_prefix_ (const mulib_readline_t *r)
{
  size_t p = r->cursor;
  while (p && !strchr (" \t()[]{};,+*/=\"'", r->line[p - 1])) --p;
  return p;
}
int mulib_readline_complete (mulib_readline_t *r, mulib_readline_complete_fn fn, void *user, size_t *start)
{
  size_t i, p, n;
  int rc = 0;
  if (!r || !fn) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  p = mulib_readline_prefix_ (r); n = r->cursor - p; if (start) *start = p;
  r->busy = 1;
  for (i = 0; i < r->completion_count && !rc; ++i)
    if (!strncmp (r->completions[i].name, r->line + p, n)) rc = fn (user, r->completions[i].name, r->completions[i].help);
  r->busy = 0; return rc;
}
static int mulib_readline_replace_ (mulib_readline_t *r, size_t first, size_t last, const char *s, size_t n)
{
  char *next;
  size_t total;
  if (n > r->limits.line_bytes - (r->length - (last - first))) return MULIB_READLINE_LIMIT;
  total = r->length - (last - first) + n;
  next = (char *)malloc (total + 1); if (!next) return MULIB_READLINE_MEMORY;
  memcpy (next, r->line, first); if (n) memcpy (next + first, s, n);
  memcpy (next + first + n, r->line + last, r->length - last + 1);
  free (r->line); r->line = next; r->length = total; r->cursor = first + n; return 0;
}
int mulib_readline_accept (mulib_readline_t *r, size_t index)
{
  size_t i, p, n;
  if (!r) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  p = mulib_readline_prefix_ (r); n = r->cursor - p;
  for (i = 0; i < r->completion_count; ++i)
    if (!strncmp (r->completions[i].name, r->line + p, n) && !index--)
      return mulib_readline_replace_ (r, p, r->cursor, r->completions[i].name, strlen (r->completions[i].name));
  return MULIB_READLINE_NOT_FOUND;
}
int mulib_readline_dispatch (mulib_readline_t *r, const char *line,
    const char **action, const char **arguments, char **expansion)
{
  size_t i, n;
  const char *args;
  if (!r || !line || !action || !arguments || !expansion) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  if (strlen (line) > r->limits.line_bytes) return MULIB_READLINE_LIMIT;
  if (*line != ':') return MULIB_READLINE_NOT_FOUND;
  n = strcspn (line + 1, " \t"); args = line + 1 + n; while (*args == ' ' || *args == '\t') ++args;
  for (i = 0; i < r->directive_count; ++i)
    {
      mulib_readline_item_ *v = r->directives + i;
      if (strlen (v->name) == n && !memcmp (v->name, line + 1, n))
        {
          char *result = NULL;
          if (v->expansion)
            {
              size_t at, used = 0, a = strlen (args), length = strlen (v->value);
              for (at = 0; at < length; ++at)
                {
                  size_t add = v->value[at] == '$' && v->value[at + 1] == '*' ? (++at, a) : 1;
                  if (add > r->limits.line_bytes - used) return MULIB_READLINE_LIMIT;
                  used += add;
                }
              result = (char *)malloc (used + 1); if (!result) return MULIB_READLINE_MEMORY;
              used = 0;
              for (at = 0; at < length; ++at)
                if (v->value[at] == '$' && v->value[at + 1] == '*')
                  { memcpy (result + used, args, a); used += a; ++at; }
                else result[used++] = v->value[at];
              result[used] = 0;
            }
          *action = v->expansion ? NULL : v->value; *arguments = args; *expansion = result;
          return MULIB_READLINE_DIRECTIVE;
        }
    }
  return MULIB_READLINE_NOT_FOUND;
}
static const char *mulib_readline_string_ (const mulib_json_t *v, const char *key)
{
  size_t n;
  const char *s = mulib_json_text (mulib_json_get (v, key), &n);
  return s && !memchr (s, 0, n) ? s : NULL;
}
static int mulib_readline_optional_ (const mulib_json_t *v, const char *key)
{ return !mulib_json_get (v, key) || mulib_readline_string_ (v, key) != NULL; }
int mulib_readline_load_memory (mulib_readline_t *r, mulib_readline_profile_kind_t kind,
    const char *text, size_t length, size_t *error_line)
{
  mulib_json_error_t error = { 0, 0, 0, NULL };
  mulib_json_t *json, *array;
  size_t rules, ac, dirs, bytes, i;
  int rc = 0;
  if (error_line) *error_line = 0;
  if (!r || (!text && length) || kind < 0 || kind > MULIB_READLINE_DIRECTIVES) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  if (length > r->limits.config_bytes) return MULIB_READLINE_LIMIT;
  json = mulib_json_parse (text, length, &error);
  if (!json)
    {
      if (error_line) *error_line = error.line;
      return error.message && (!strcmp (error.message, "out of memory")
          || !strcmp (error.message, "capture allocation overflow")) ? MULIB_READLINE_MEMORY : MULIB_READLINE_INVALID;
    }
  array = mulib_json_get (json, kind == MULIB_READLINE_SYNTAX ? "rules" : kind == MULIB_READLINE_COMPLETIONS ? "items" : "directives");
  if (mulib_json_typeof (json) != MULIB_JSON_OBJECT || mulib_json_numeric (mulib_json_get (json, "version")) != 1
      || mulib_json_typeof (array) != MULIB_JSON_ARRAY) { mulib_json_destroy (json); return MULIB_READLINE_INVALID; }
  rules = r->rule_count; ac = r->completion_count; dirs = r->directive_count; bytes = r->config_bytes;
  for (i = 0; i < mulib_json_size (array) && !rc; ++i)
    {
      const mulib_json_t *v = mulib_json_at (array, i);
      if (mulib_json_typeof (v) != MULIB_JSON_OBJECT) { rc = MULIB_READLINE_INVALID; break; }
      if (kind == MULIB_READLINE_SYNTAX)
        {
          static const char *const kinds[] = { "word", "line", "span", "number" };
          static const char *const styles[] = { "plain", "keyword", "string", "comment", "number", "type", "function", "directive" };
          const char *k = mulib_readline_string_ (v, "kind"), *s = mulib_readline_string_ (v, "style");
          const char *escape = mulib_readline_string_ (v, "escape");
          size_t ki, si;
          for (ki = 0; ki < 4 && (!k || strcmp (k, kinds[ki])); ++ki) {}
          for (si = 0; si < 8 && (!s || strcmp (s, styles[si])); ++si) {}
          if (ki == 4 || si == 8 || (escape && strlen (escape) != 1)
              || !mulib_readline_optional_ (v, "escape") || !mulib_readline_optional_ (v, "start")
              || !mulib_readline_optional_ (v, "end")) rc = MULIB_READLINE_INVALID;
          else rc = mulib_readline_rule (r, (mulib_readline_rule_kind_t)ki, (mulib_readline_style_t)si,
              mulib_readline_string_ (v, "start"), mulib_readline_string_ (v, "end"), escape ? *escape : 0);
        }
      else if (kind == MULIB_READLINE_COMPLETIONS)
        rc = !mulib_readline_optional_ (v, "description") ? MULIB_READLINE_INVALID
            : mulib_readline_completion (r, mulib_readline_string_ (v, "text"), mulib_readline_string_ (v, "description"));
      else rc = !mulib_readline_optional_ (v, "action") || !mulib_readline_optional_ (v, "expansion")
          || !mulib_readline_optional_ (v, "help") ? MULIB_READLINE_INVALID
          : mulib_readline_directive (r, mulib_readline_string_ (v, "name"), mulib_readline_string_ (v, "action"),
              mulib_readline_string_ (v, "expansion"), mulib_readline_string_ (v, "help"));
    }
  mulib_json_destroy (json);
  if (rc) { mulib_readline_trim_ (r, rules, ac, dirs); r->config_bytes = bytes; }
  return rc;
}
int mulib_readline_load (mulib_readline_t *r, const char *path, size_t *error_line)
{
  FILE *f;
  char *data;
  size_t n, size = 0;
  const char *ext;
  mulib_readline_profile_kind_t kind;
  int rc;
  if (error_line) *error_line = 0;
  if (!r || !path || !(ext = strrchr (path, '.'))) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  if (!strcmp (ext, ".mlsyn")) kind = MULIB_READLINE_SYNTAX;
  else if (!strcmp (ext, ".mlac")) kind = MULIB_READLINE_COMPLETIONS;
  else if (!strcmp (ext, ".mldir")) kind = MULIB_READLINE_DIRECTIVES;
  else return MULIB_READLINE_INVALID;
  if (r->limits.config_bytes == SIZE_MAX) return MULIB_READLINE_LIMIT;
  f = fopen (path, "rb"); if (!f) return MULIB_READLINE_IO;
  data = (char *)malloc (r->limits.config_bytes + 1);
  if (!data) { fclose (f); return MULIB_READLINE_MEMORY; }
  while (size <= r->limits.config_bytes && (n = fread (data + size, 1, r->limits.config_bytes + 1 - size, f)) != 0) size += n;
  rc = ferror (f) ? MULIB_READLINE_IO : size > r->limits.config_bytes ? MULIB_READLINE_LIMIT
      : mulib_readline_load_memory (r, kind, data, size, error_line);
  fclose (f); free (data); return rc;
}
int mulib_readline_history_add (mulib_readline_t *r, const char *s)
{
  size_t n, drop = 0, bytes, count;
  char *copy, **next;
  if (!r || !s || !mulib_readline_clean_ (s, strlen (s))) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  n = strlen (s);
  if (!n || (r->history_count && !strcmp (s, r->history[r->history_count - 1]))) return 0;
  if (n > r->limits.line_bytes || n >= r->limits.history_bytes) return MULIB_READLINE_LIMIT;
  copy = mulib_readline_copy_ (s, n); if (!copy) return MULIB_READLINE_MEMORY;
  bytes = r->history_bytes; count = r->history_count;
  while (count >= r->limits.history_entries || n + 1 > r->limits.history_bytes - bytes)
    { bytes -= strlen (r->history[drop++]) + 1; --count; }
  next = drop ? r->history : (char **)realloc (r->history, (r->history_count + 1) * sizeof (*next));
  if (!next) { free (copy); return MULIB_READLINE_MEMORY; }
  r->history = next;
  for (size_t i = 0; i < drop; ++i) free (next[i]);
  memmove (next, next + drop, count * sizeof (*next)); next[count++] = copy;
  r->history_count = r->history_at = count; r->history_bytes = bytes + n + 1;
  return 0;
}
static size_t mulib_readline_previous_ (const char *s, size_t at)
{ if (at) { --at; while (at && ((unsigned char)s[at] & 0xc0) == 0x80) --at; } return at; }
static size_t mulib_readline_next_ (const char *s, size_t n, size_t at)
{ if (at < n) { ++at; while (at < n && ((unsigned char)s[at] & 0xc0) == 0x80) ++at; } return at; }
int mulib_readline_edit (mulib_readline_t *r, const mulib_termkey_event_t *e)
{
  unsigned char encoded[4];
  size_t n;
  if (!r || !e) return MULIB_READLINE_INVALID;
  if (r->busy) return MULIB_READLINE_BUSY;
  if (e->type == MULIB_TERMKEY_PASTE_BEGIN) { r->paste_length = 0; return 0; }
  if (e->type == MULIB_TERMKEY_PASTE_END)
    { int incomplete = r->paste_length != 0; r->paste_length = 0; return incomplete ? MULIB_READLINE_INVALID : 0; }
  if (e->type == MULIB_TERMKEY_PASTE_TEXT)
    {
      char clean[MULIB_TERMKEY_CAPACITY + 4];
      unsigned char pending[4];
      size_t held = r->paste_length, written = 0;
      int rc;
      if (e->length > MULIB_TERMKEY_CAPACITY) return MULIB_READLINE_INVALID;
      memcpy (pending, r->paste_pending, held);
      for (n = 0; n < e->length; ++n)
        {
          size_t need;
          unsigned char ch = e->raw[n];
          if (!held && (ch < 32 || ch == 127)) ch = ' ';
          pending[held++] = ch;
          need = pending[0] < 128 ? 1 : pending[0] >= 0xc2 && pending[0] <= 0xdf ? 2
              : pending[0] >= 0xe0 && pending[0] <= 0xef ? 3 : pending[0] >= 0xf0 && pending[0] <= 0xf4 ? 4 : 0;
          if (!need) return MULIB_READLINE_INVALID;
          if (held == need)
            {
              if (!mulib_readline_clean_ ((const char *)pending, held)) return MULIB_READLINE_INVALID;
              memcpy (clean + written, pending, held); written += held; held = 0;
            }
        }
      rc = written ? mulib_readline_replace_ (r, r->cursor, r->cursor, clean, written) : 0;
      if (!rc) { memcpy (r->paste_pending, pending, held); r->paste_length = held; }
      return rc;
    }
  if (e->type != MULIB_TERMKEY_KEY) return 0;
  if (e->key == MULIB_TERMKEY_CHAR && (e->modifiers & MULIB_TERMKEY_CTRL))
    {
      switch (e->codepoint)
        {
        case 'a': r->cursor = 0; return 0;
        case 'e': r->cursor = r->length; return 0;
        case 'c': return MULIB_READLINE_INTERRUPT;
        case 'd': if (!r->length) return MULIB_READLINE_EOF;
          return mulib_readline_replace_ (r, r->cursor, mulib_readline_next_ (r->line, r->length, r->cursor), "", 0);
        case 'k': return mulib_readline_replace_ (r, r->cursor, r->length, "", 0);
        case 'u': return mulib_readline_replace_ (r, 0, r->cursor, "", 0);
        case 'w': n = r->cursor; while (n && r->line[n - 1] == ' ') --n;
          while (n && r->line[n - 1] != ' ') n = mulib_readline_previous_ (r->line, n);
          return mulib_readline_replace_ (r, n, r->cursor, "", 0);
        default: return 0;
        }
    }
  switch (e->key)
    {
    case MULIB_TERMKEY_ENTER: return MULIB_READLINE_LINE;
    case MULIB_TERMKEY_LEFT: r->cursor = mulib_readline_previous_ (r->line, r->cursor); break;
    case MULIB_TERMKEY_RIGHT: r->cursor = mulib_readline_next_ (r->line, r->length, r->cursor); break;
    case MULIB_TERMKEY_HOME: r->cursor = 0; break;
    case MULIB_TERMKEY_END: r->cursor = r->length; break;
    case MULIB_TERMKEY_BACKSPACE: return mulib_readline_replace_ (r, mulib_readline_previous_ (r->line, r->cursor), r->cursor, "", 0);
    case MULIB_TERMKEY_DELETE: return mulib_readline_replace_ (r, r->cursor, mulib_readline_next_ (r->line, r->length, r->cursor), "", 0);
    case MULIB_TERMKEY_TAB: { int rc = mulib_readline_accept (r, 0); return rc == MULIB_READLINE_NOT_FOUND ? 0 : rc; }
    case MULIB_TERMKEY_UP: case MULIB_TERMKEY_DOWN:
      {
        size_t at = r->history_at;
        int rc;
        if (e->key == MULIB_TERMKEY_UP)
          {
            if (!at) break;
            if (at == r->history_count)
              { char *draft = mulib_readline_copy_ (r->line, r->length); if (!draft) return MULIB_READLINE_MEMORY; free (r->draft); r->draft = draft; }
            --at;
          }
        else { if (at == r->history_count) break; ++at; }
        { const char *s = at == r->history_count ? (r->draft ? r->draft : "") : r->history[at]; rc = mulib_readline_set (r, s, strlen (s)); }
        if (!rc) r->history_at = at;
        return rc;
      }
    case MULIB_TERMKEY_CHAR:
      if (e->modifiers & (MULIB_TERMKEY_ALT | MULIB_TERMKEY_SUPER)) break;
      n = mulib_utf8_encode (e->codepoint, encoded, sizeof (encoded));
      if (!n || !mulib_readline_clean_ ((const char *)encoded, n)) return MULIB_READLINE_INVALID;
      return mulib_readline_replace_ (r, r->cursor, r->cursor, (const char *)encoded, n);
    default: break;
    }
  return 0;
}
static unsigned mulib_readline_width_ (uint32_t c)
{
  if ((c >= 0x300 && c <= 0x36f) || (c >= 0xfe00 && c <= 0xfe0f) || c == 0x200d) return 0;
  return (c >= 0x1100 && (c <= 0x115f || (c >= 0x2e80 && c <= 0xa4cf)
      || (c >= 0xac00 && c <= 0xd7a3) || (c >= 0xf900 && c <= 0xfaff)
      || (c >= 0xff01 && c <= 0xff60) || c >= 0x1f300)) ? 2 : 1;
}
typedef struct mulib_readline_render_
{ mulib_strbuf_t bytes; const char *line; size_t first, last; int failed; } mulib_readline_render_;
static int mulib_readline_render_span_ (void *user, size_t at, size_t n, mulib_readline_style_t style)
{
  static const char *const colors[] = { "\033[0m", "\033[35m", "\033[32m", "\033[90m", "\033[36m", "\033[34m", "\033[33m", "\033[95m" };
  mulib_readline_render_ *v = (mulib_readline_render_ *)user;
  size_t end = at + n;
  if (at < v->first) at = v->first;
  if (end > v->last) end = v->last;
  if (at >= end) return 0;
  if (!mulib_strbuf_append (&v->bytes, colors[style]) || !mulib_strbuf_append_n (&v->bytes, v->line + at, end - at)) return v->failed = MULIB_READLINE_MEMORY;
  return 0;
}
int mulib_readline_render (mulib_readline_t *r, const char *prompt, unsigned columns, char **output)
{
  mulib_readline_render_ v;
  size_t at, used, plen, first = 0, last;
  size_t cursor_cols = 0;
  unsigned width = 0, available;
  uint32_t cp;
  char move[48];
  int rc;
  if (!r || !prompt || !output || columns < 8) return MULIB_READLINE_INVALID;
  plen = strlen (prompt); if (plen > columns - 4) return MULIB_READLINE_LIMIT;
  for (at = 0; at < plen; ++at) if ((unsigned char)prompt[at] < 32 || (unsigned char)prompt[at] > 126) return MULIB_READLINE_INVALID;
  available = columns - (unsigned)plen - 1;
  for (at = 0; at < r->cursor; at += used)
    { (void)mulib_utf8_decode ((unsigned char *)r->line + at, r->length - at, &cp, &used); cursor_cols += mulib_readline_width_ (cp); }
  while (cursor_cols >= available && first < r->cursor)
    { (void)mulib_utf8_decode ((unsigned char *)r->line + first, r->length - first, &cp, &used); cursor_cols -= mulib_readline_width_ (cp); first += used; }
  last = first;
  while (last < r->length)
    {
      unsigned w;
      (void)mulib_utf8_decode ((unsigned char *)r->line + last, r->length - last, &cp, &used); w = mulib_readline_width_ (cp);
      if (w > available - width) break;
      width += w; last += used;
    }
  memset (&v, 0, sizeof (v)); mulib_strbuf_init (&v.bytes); v.line = r->line; v.first = first; v.last = last;
  rc = !mulib_strbuf_append (&v.bytes, "\r\033[2K") || !mulib_strbuf_append (&v.bytes, prompt) ? MULIB_READLINE_MEMORY : 0;
  if (!rc) rc = mulib_readline_highlight (r, r->line, r->length, mulib_readline_render_span_, &v);
  if (plen + cursor_cols) (void)snprintf (move, sizeof (move), "\033[0m\r\033[%uC", (unsigned)(plen + cursor_cols));
  else strcpy (move, "\033[0m\r");
  if (!rc && !mulib_strbuf_append (&v.bytes, move)) rc = MULIB_READLINE_MEMORY;
  if (!rc) { *output = mulib_readline_copy_ (v.bytes.data, v.bytes.length); if (!*output) rc = MULIB_READLINE_MEMORY; }
  mulib_strbuf_free (&v.bytes); return rc;
}
int mulib_readline_read (mulib_readline_t *r, int input, int output, const char *prompt, char **line)
{
  if (!r || !line || *line || !prompt || input < 0 || output < 0) return MULIB_READLINE_INVALID;
#if defined(__unix__) || defined(__APPLE__)
  {
    mulib_termutils_raw_t raw = MULIB_TERMUTILS_RAW_INIT;
    mulib_termkey_t decoder;
    int tty = isatty (input) && isatty (output), rc = mulib_readline_set (r, "", 0);
    size_t written;
    if (rc) return rc;
    r->history_at = r->history_count; free (r->draft); r->draft = NULL;
    mulib_termkey_init (&decoder);
    if (tty && mulib_termutils_raw_begin (&raw, input)) return MULIB_READLINE_IO;
    if (tty) (void)mulib_termutils_write (output, "\033[?2004h", 8, &written);
    while (!rc)
      {
        unsigned char c;
        ssize_t got;
        if (tty)
          {
            unsigned columns = 80, rows;
            char *rendered = NULL;
            (void)mulib_termutils_size (output, &columns, &rows);
            rc = mulib_readline_render (r, prompt, columns, &rendered);
            if (!rc && mulib_termutils_write (output, rendered, strlen (rendered), &written)) rc = MULIB_READLINE_IO;
            free (rendered); if (rc) break;
          }
        do { got = read (input, &c, 1); } while (got < 0 && errno == EINTR);
        if (got <= 0) { rc = got < 0 ? MULIB_READLINE_IO : r->length ? MULIB_READLINE_LINE : MULIB_READLINE_EOF; break; }
        if (!tty)
          {
            if (c == '\n') { rc = MULIB_READLINE_LINE; break; }
            if (c == '\r') continue;
            if (c < 32 || c == 127) { rc = MULIB_READLINE_INVALID; break; }
            rc = mulib_readline_replace_ (r, r->length, r->length, (const char *)&c, 1);
          }
        else
          {
            mulib_termkey_event_t e;
            int flush = 0;
            (void)mulib_termkey_feed (&decoder, &c, 1);
            if (decoder.length && decoder.pending[0] == 27)
              { struct pollfd p = { input, POLLIN, 0 }; flush = poll (&p, 1, 35) == 0; }
            while (mulib_termkey_next (&decoder, &e, flush) == 1 && !rc) rc = mulib_readline_edit (r, &e);
          }
      }
    if (tty)
      {
        (void)mulib_termutils_write (output, "\033[0m\033[?2004l\r\n", 14, &written);
        if (mulib_termutils_raw_end (&raw)) { free (raw.saved); rc = MULIB_READLINE_IO; }
      }
    if (!mulib_readline_clean_ (r->line, r->length))
      { r->line[0] = 0; r->length = r->cursor = 0; return rc < 0 ? rc : MULIB_READLINE_INVALID; }
    if (rc == MULIB_READLINE_LINE)
      {
        *line = mulib_readline_copy_ (r->line, r->length);
        if (!*line) return MULIB_READLINE_MEMORY;
        (void)mulib_readline_history_add (r, r->line);
      }
    return rc;
  }
#else
  return MULIB_READLINE_UNSUPPORTED;
#endif
}
#endif
