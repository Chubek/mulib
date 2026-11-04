/** @file mulib-termstyle.h
 * @brief Explicit, terminfo-backed terminal attributes and indexed colors.
 * Define MULIB_IMPL_TERMSTYLE, TERMINFO and STRBUF. No implicit ANSI, TERM,
 * isatty or NO_COLOR policy; caller selects a record and enables styling. */
#ifndef MULIB_TERMSTYLE_H
#define MULIB_TERMSTYLE_H
#include "mulib-terminfo.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_termstyle_attribute {
  MULIB_TERMSTYLE_BOLD = 1, MULIB_TERMSTYLE_DIM = 2,
  MULIB_TERMSTYLE_ITALIC = 4, MULIB_TERMSTYLE_UNDERLINE = 8,
  MULIB_TERMSTYLE_REVERSE = 16
};
/** @brief Borrowed record must outlive this context. Disabled contexts emit no escapes. */
typedef struct mulib_termstyle {
  const mulib_terminfo_t *info;
  int enabled;
} mulib_termstyle_t;
/** @brief Color -1 means terminal default; other values are indexed colors. */
typedef struct mulib_termstyle_spec {
  unsigned attributes;
  int32_t foreground, background;
} mulib_termstyle_spec_t;
/** @brief Initialize context; NULL info forces plain output. */
void mulib_termstyle_init (mulib_termstyle_t *s, const mulib_terminfo_t *info, int enabled);
/** @brief Append reset plus requested attributes/colors atomically; 1 success,
 * 0 invalid, unavailable capability, expansion error or allocation failure. */
int mulib_termstyle_apply (const mulib_termstyle_t *s, const mulib_termstyle_spec_t *spec, mulib_strbuf_t *out);
/** @brief Append sgr0, or nothing when disabled; atomic, 1/0. */
int mulib_termstyle_reset (const mulib_termstyle_t *s, mulib_strbuf_t *out);
/** @brief Append styled text plus reset atomically. Text is not escaped; may alias out. */
int mulib_termstyle_text (const mulib_termstyle_t *s, const mulib_termstyle_spec_t *spec,
    const char *text, size_t length, mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TERMSTYLE) && !defined(MULIB_TERMSTYLE_IMPLEMENTED)
#define MULIB_TERMSTYLE_IMPLEMENTED
void mulib_termstyle_init (mulib_termstyle_t *s, const mulib_terminfo_t *info, int enabled)
{ if (s) { s->info = info; s->enabled = info && enabled; } }
int mulib_termstyle_reset (const mulib_termstyle_t *s, mulib_strbuf_t *out)
{
  if (!s || !out) return 0;
  return !s->enabled || (s->info && mulib_terminfo_emit (s->info, "sgr0", NULL, out));
}
int mulib_termstyle_apply (const mulib_termstyle_t *s, const mulib_termstyle_spec_t *spec, mulib_strbuf_t *out)
{
  static const char *const caps[] = { "bold", "dim", "sitm", "smul", "rev" };
  mulib_strbuf_t tmp = { 0 };
  int32_t params[9] = { 0 }, colors;
  unsigned i;
  int ok = 0;
  if (!s || !spec || !out || (spec->attributes & ~31u)
      || spec->foreground < -1 || spec->background < -1) return 0;
  if (!s->enabled) return 1;
  if (!s->info || !mulib_termstyle_reset (s, &tmp)) goto done;
  for (i = 0; i < 5; ++i)
    if ((spec->attributes & (1u << i)) && !mulib_terminfo_emit (s->info, caps[i], NULL, &tmp)) goto done;
  colors = mulib_terminfo_num (s->info, "colors");
  if (spec->foreground >= 0) {
    params[0] = spec->foreground;
    if (params[0] >= colors || !mulib_terminfo_emit (s->info, "setaf", params, &tmp)) goto done;
  }
  if (spec->background >= 0) {
    params[0] = spec->background;
    if (params[0] >= colors || !mulib_terminfo_emit (s->info, "setab", params, &tmp)) goto done;
  }
  ok = mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
int mulib_termstyle_text (const mulib_termstyle_t *s, const mulib_termstyle_spec_t *spec,
    const char *text, size_t n, mulib_strbuf_t *out)
{
  mulib_strbuf_t tmp = { 0 };
  int ok;
  if (!out || (!text && n)) return 0;
  ok = mulib_termstyle_apply (s, spec, &tmp) && mulib_strbuf_append_n (&tmp, text, n)
      && mulib_termstyle_reset (s, &tmp) && mulib_strbuf_append_n (out, tmp.data, tmp.length);
  mulib_strbuf_free (&tmp); return ok;
}
#endif
