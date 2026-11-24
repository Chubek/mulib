/** @file mulib-superinstr.h
 * @brief Exact straight-line assembly superinstructions emitted as GAS macros.
 * Define MULIB_IMPL_SUPERINSTR, MULIB_IMPL_MIR and MULIB_IMPL_STRBUF.
 */
#ifndef MULIB_SUPERINSTR_H
#define MULIB_SUPERINSTR_H
#include "mulib-mir.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_superinstr_options {
  size_t min_lines, max_lines, min_uses, text_limit, work_limit;
} mulib_superinstr_options_t;
typedef struct mulib_superinstr_result {
  char *text;
  size_t length, macros, uses;
} mulib_superinstr_result_t;
/** Defaults: 2..8 instruction lines, at least 2 occurrences, 1 MiB output,
 * 16M scan/comparison work units. Macro expansion preserves instruction bytes and ABI effects;
 * this factors assembly source, not executable size. */
mulib_superinstr_options_t mulib_superinstr_defaults (void);
/** Factor exact nonoverlapping repeated sequences from MuLIB/GAS assembly.
 * Labels, directives, comments and macro escapes cannot enter a body. Existing
 * macro modes/definitions, inline label statements and assembler control
 * directives are rejected. symbol namespaces temporary macro names;
 * definitions are purged at the end. Input is length-delimited and immutable.
 * Output must be zero; all failures preserve it. */
mulib_mir_status_t mulib_superinstr_pack (const char *, size_t length,
    const char *symbol, const mulib_superinstr_options_t *, mulib_superinstr_result_t *);
/** Release returned text and zero the result. */
void mulib_superinstr_free (mulib_superinstr_result_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SUPERINSTR) && !defined(MULIB_SUPERINSTR_IMPLEMENTED)
#define MULIB_SUPERINSTR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_superinstr_line_ { size_t first, length, group, span; int eligible; }
  mulib_superinstr_line_;
mulib_superinstr_options_t mulib_superinstr_defaults (void)
{ mulib_superinstr_options_t o = {2, 8, 2, 1048576, 16777216}; return o; }
void mulib_superinstr_free (mulib_superinstr_result_t *r)
{ if (r) { free (r->text); memset (r, 0, sizeof (*r)); } }
static mulib_mir_status_t mulib_superinstr_append_ (mulib_strbuf_t *b,
    const char *text, size_t n, size_t limit)
{
  if (b->length > limit || n > limit - b->length || n == SIZE_MAX - b->length) return MULIB_MIR_RESOURCE_LIMIT;
  return mulib_strbuf_append_n (b, text, n) ? MULIB_MIR_OK : MULIB_MIR_NO_MEMORY;
}
static int mulib_superinstr_work_ (size_t *remaining, size_t amount)
{
  if (amount > *remaining) return 0;
  *remaining -= amount; return 1;
}
static int mulib_superinstr_word_ (const char *text, size_t length, const char *word)
{
  if (strlen (word) != length) return 0;
  for (size_t k = 0; k < length; ++k) {
    unsigned char c = (unsigned char)text[k];
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    if (c != (unsigned char)word[k]) return 0;
  }
  return 1;
}
static int mulib_superinstr_directive_ (const char *text, size_t length)
{
  static const char *const allowed[] = {".text", ".globl", ".global", ".type", ".size",
    ".section", ".align", ".balign", ".p2align", ".file", ".ident"};
  for (size_t k = 0; k < sizeof (allowed) / sizeof (*allowed); ++k)
    if (mulib_superinstr_word_ (text, length, allowed[k])) return 1;
  return 0;
}
mulib_mir_status_t mulib_superinstr_pack (const char *text, size_t length,
    const char *symbol, const mulib_superinstr_options_t *options, mulib_superinstr_result_t *out)
{
  mulib_superinstr_options_t defaults = mulib_superinstr_defaults ();
  const mulib_superinstr_options_t *o = options ? options : &defaults;
  mulib_superinstr_line_ *lines = NULL;
  mulib_superinstr_result_t result = {NULL, 0, 0, 0};
  mulib_strbuf_t buffer = {0};
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  size_t count = 0, first = 0, work = o->work_limit;
  char name[256];
  if (!out || out->text || out->length || out->macros || out->uses || (!text && length)
      || !mulib_mir_native_name (symbol) || o->min_lines < 2
      || o->max_lines < o->min_lines || o->min_uses < 2) goto done;
  if (length > o->text_limit) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
  /* Charge input/line scans as well as matching, including inputs that cannot
   * produce any eligible span. Avoid multiplication before the bound check. */
  if (length > work / 8 || !mulib_superinstr_work_ (&work, length * 8)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
  if (length && (memchr (text, 0, length) || text[length - 1] != '\n')) goto done;
  for (size_t k = 0; k < length; ++k) {
    if (text[k] == '\\' || text[k] == ';'
        || ((unsigned char)text[k] < 32 && text[k] != '\t' && text[k] != '\n'
            && !(text[k] == '\r' && k + 1 < length && text[k + 1] == '\n'))
        || (text[k] == '/' && k + 1 < length && (text[k + 1] == '*' || text[k + 1] == '/')))
      { s = MULIB_MIR_UNSUPPORTED; goto done; }
    if (text[k] == '\n') ++count;
  }
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (count > SIZE_MAX / sizeof (*lines)) goto done;
  lines = (mulib_superinstr_line_ *)calloc (count ? count : 1, sizeof (*lines));
  s = MULIB_MIR_NO_MEMORY; if (!lines) goto done;
  count = 0;
  for (size_t k = 0; k < length; ++k) if (text[k] == '\n') {
    size_t start = first, token;
    mulib_superinstr_line_ *l = &lines[count++];
    l->first = first; l->length = k + 1 - first; l->group = SIZE_MAX;
    while (start < k && (text[start] == ' ' || text[start] == '\t')) ++start;
    token = start;
    while (token < k && text[token] != ' ' && text[token] != '\t' && text[token] != '\r') ++token;
    if (start < token && text[start] != '#' && memchr (text + start, ':', token - start)) {
      size_t rest = token;
      if (text[token - 1] != ':') { s = MULIB_MIR_UNSUPPORTED; goto done; }
      while (rest < k && (text[rest] == ' ' || text[rest] == '\t' || text[rest] == '\r')) ++rest;
      if (rest < k && text[rest] != '#') { s = MULIB_MIR_UNSUPPORTED; goto done; }
    }
    if (start < token && text[token - 1] != ':') {
      if (mulib_superinstr_word_ (text + start, token - start, ".macro")) { s = MULIB_MIR_INVALID; goto done; }
      if ((text[start] == '.' && !mulib_superinstr_directive_ (text + start, token - start))
          || (token - start >= 9 && !memcmp (text + start, "mulib_si_", 9)))
        { s = MULIB_MIR_UNSUPPORTED; goto done; }
    }
    l->eligible = start > first && start < k && ((text[start] >= 'a' && text[start] <= 'z')
        || (text[start] >= 'A' && text[start] <= 'Z'))
        && !memchr (text + start, ':', k - start) && !memchr (text + start, '\\', k - start)
        && !memchr (text + start, '#', k - start) && !memchr (text + start, ';', k - start);
    first = k + 1;
  }
  for (size_t i = 0; i < count; ++i) {
    size_t max = o->max_lines < count - i ? o->max_lines : count - i;
    if (!mulib_superinstr_work_ (&work, 1)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    if (!lines[i].eligible || lines[i].group != SIZE_MAX) continue;
    for (size_t span = max; span >= o->min_lines; --span) {
      size_t uses = 0, bytes = lines[i + span - 1].first + lines[i + span - 1].length - lines[i].first;
      int eligible = 1;
      if (!mulib_superinstr_work_ (&work, 1)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
      for (size_t a = 0; a < span; ++a) {
        if (!mulib_superinstr_work_ (&work, 1)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
        if (!lines[i + a].eligible || lines[i + a].group != SIZE_MAX) { eligible = 0; break; }
      }
      if (!eligible) continue;
      for (size_t j = i; span <= count - j;) {
        int match = 1;
        for (size_t a = 0; a < span; ++a) {
          if (!mulib_superinstr_work_ (&work, 1)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
          if (!lines[j + a].eligible || lines[j + a].group != SIZE_MAX
              || lines[i + a].length != lines[j + a].length) { match = 0; break; }
          if (!mulib_superinstr_work_ (&work, lines[i + a].length)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
          if (memcmp (text + lines[i + a].first, text + lines[j + a].first, lines[i + a].length)) { match = 0; break; }
        }
        if (match) { ++uses; j += span; } else ++j;
      }
      if (uses < o->min_uses) continue;
      /* Avoid increasing source size just to create a nominal superinstruction. */
      if (bytes <= strlen (symbol) + 40 || uses > SIZE_MAX - 3
          || uses + 3 > SIZE_MAX / (strlen (symbol) + 40)
          || bytes > SIZE_MAX / (uses - 1)
          || bytes * (uses - 1) <= (uses + 3) * (strlen (symbol) + 40)) continue;
      for (size_t j = i; span <= count - j;) {
        int match = 1;
        for (size_t a = 0; a < span; ++a) {
          if (!mulib_superinstr_work_ (&work, 1)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
          if (!lines[j + a].eligible || lines[j + a].group != SIZE_MAX
              || lines[i + a].length != lines[j + a].length) { match = 0; break; }
          if (!mulib_superinstr_work_ (&work, lines[i + a].length)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
          if (memcmp (text + lines[i + a].first, text + lines[j + a].first, lines[i + a].length)) { match = 0; break; }
        }
        if (match) {
          if (!mulib_superinstr_work_ (&work, span)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
          for (size_t a = 0; a < span; ++a) lines[j + a].group = result.macros;
          lines[j].span = span; ++result.uses; j += span;
        } else ++j;
      }
      {
        int n = snprintf (name, sizeof (name), "\t.macro mulib_si_%s_%zu\n", symbol, result.macros++);
        if (n < 0 || (size_t)n >= sizeof (name)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
        s = mulib_superinstr_append_ (&buffer, name, (size_t)n, o->text_limit); if (s != MULIB_MIR_OK) goto done;
        s = mulib_superinstr_append_ (&buffer, text + lines[i].first, bytes, o->text_limit); if (s != MULIB_MIR_OK) goto done;
        s = mulib_superinstr_append_ (&buffer, "\t.endm\n", 7, o->text_limit); if (s != MULIB_MIR_OK) goto done;
      }
      break;
    }
  }
  for (size_t i = 0; i < count;) {
    if (lines[i].group == SIZE_MAX) {
      s = mulib_superinstr_append_ (&buffer, text + lines[i].first, lines[i].length, o->text_limit); ++i;
    } else {
      int n = snprintf (name, sizeof (name), "\tmulib_si_%s_%zu\n", symbol, lines[i].group);
      if (n < 0 || (size_t)n >= sizeof (name) || !lines[i].span) { s = MULIB_MIR_INVALID; goto done; }
      s = mulib_superinstr_append_ (&buffer, name, (size_t)n, o->text_limit); i += lines[i].span;
    }
    if (s != MULIB_MIR_OK) goto done;
  }
  for (size_t i = 0; i < result.macros; ++i) {
    int n = snprintf (name, sizeof (name), "\t.purgem mulib_si_%s_%zu\n", symbol, i);
    if (n < 0 || (size_t)n >= sizeof (name)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    s = mulib_superinstr_append_ (&buffer, name, (size_t)n, o->text_limit); if (s != MULIB_MIR_OK) goto done;
  }
  s = mulib_superinstr_append_ (&buffer, "", 0, o->text_limit); if (s != MULIB_MIR_OK) goto done;
  result.text = buffer.data; result.length = buffer.length; memset (&buffer, 0, sizeof (buffer));
  *out = result; s = MULIB_MIR_OK;
done:
  free (lines); mulib_strbuf_free (&buffer); return s;
}
#ifdef __cplusplus
}
#endif
#endif
