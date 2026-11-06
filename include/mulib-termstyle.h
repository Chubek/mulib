/**
 * @file mulib-termstyle.h
 * @brief CSS cascade to shared terminal styles and safe terminfo output.
 * Define TERMSTYLE, CSS, XML, STRBUF, UTF8, VTERM and GRID implementation
 * switches in one TU. No terminal detection, environment lookup or I/O.
 */
#ifndef MULIB_TERMSTYLE_H
#define MULIB_TERMSTYLE_H
#include "mulib-css.h"
#include "mulib-terminfo.h"
#include "mulib-vterm.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Existing indexed-context API, retained source/behavior compatible. */
enum mulib_termstyle_attribute {
  MULIB_TERMSTYLE_BOLD = 1, MULIB_TERMSTYLE_DIM = 2,
  MULIB_TERMSTYLE_ITALIC = 4, MULIB_TERMSTYLE_UNDERLINE = 8,
  MULIB_TERMSTYLE_REVERSE = 16
};
/** @brief Borrowed capabilities; disabled contexts emit no escapes. */
typedef struct mulib_termstyle {
  const mulib_terminfo_t *info;
  int enabled;
} mulib_termstyle_t;
/** @brief Legacy indexed style; -1 is the terminal default color. */
typedef struct mulib_termstyle_spec {
  unsigned attributes;
  int32_t foreground, background;
} mulib_termstyle_spec_t;
/** @brief Initialize legacy context; NULL capabilities force plain output. */
void mulib_termstyle_init(mulib_termstyle_t *, const mulib_terminfo_t *, int enabled);
/** @brief Legacy reset/rendition, atomic, 1 success/0 failure. */
int mulib_termstyle_apply(const mulib_termstyle_t *, const mulib_termstyle_spec_t *, mulib_strbuf_t *);
/** @brief Legacy sgr0 reset, atomic, 1/0. */
int mulib_termstyle_reset(const mulib_termstyle_t *, mulib_strbuf_t *);
/** @brief Legacy literal (UNSANITIZED) bytes between apply/reset; 1/0.
 * May alias output. Use write for untrusted source text. */
int mulib_termstyle_text(const mulib_termstyle_t *, const mulib_termstyle_spec_t *,
                        const char *, size_t length, mulib_strbuf_t *);
enum mulib_termstyle_result {
  MULIB_TERMSTYLE_OK = 0, MULIB_TERMSTYLE_EINVAL = -1,
  MULIB_TERMSTYLE_ENOMEM = -2, MULIB_TERMSTYLE_ENOTSUP = -3
};
enum mulib_termstyle_flags {
  /** Explicitly permit ANSI RGB sequences; requires the ANSI capability profile. */
  MULIB_TERMSTYLE_TRUECOLOR = 1u,
  /** Emit no control sequences (text sanitization still applies). */
  MULIB_TERMSTYLE_PLAIN = 2u
};
/** @brief Resolve sheet and node style attribute using the CSS cascade.
 * Supports color, background-color, font-weight, font-style, text-decoration,
 * visibility and --term-{inverse,dim,blink}. Unknown properties/values fail.
 * parent may be NULL; missing properties inherit the terminal parent style.
 * Output unchanged on failure; optional error uses static messages. */
int mulib_termstyle_resolve(const mulib_css_sheet_t *, const mulib_xml_node_t *,
                            const mulib_term_style_t *parent,
                            mulib_term_style_t *output, mulib_xml_error_t *error);
/** @brief Apply an already cascaded declaration list to parent/defaults.
 * The list may be NULL. Same validation/transactional contracts as resolve. */
int mulib_termstyle_from_css(const mulib_css_style_t *, const mulib_term_style_t *parent,
                          mulib_term_style_t *output, mulib_xml_error_t *error);
/** @brief Append a complete reset plus rendition, atomically. NULL info forces
 * plain output. RGB is quantized to the advertised 8/16/256-color palette
 * unless TRUECOLOR is enabled. Missing required capabilities fail. */
int mulib_termstyle_emit(mulib_term_style_t, const mulib_terminfo_t *, unsigned flags,
                         mulib_strbuf_t *output);
/** @brief Append styled UTF-8 followed by a rendition/color reset, atomically.
 * Invalid UTF-8 fails. LF/TAB pass; all other C0/DEL/C1 controls become U+FFFD,
 * so source text cannot inject terminal escape sequences. */
int mulib_termstyle_write(mulib_term_style_t, const mulib_terminfo_t *, unsigned flags,
                         const char *text, size_t length, mulib_strbuf_t *output);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSTYLE) && !defined(MULIB_TERMSTYLE_IMPLEMENTED)
#define MULIB_TERMSTYLE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_termstyle_init(mulib_termstyle_t *s, const mulib_terminfo_t *info, int enabled) {
  if (s) { s->info = info; s->enabled = info && enabled; }
}
int mulib_termstyle_reset(const mulib_termstyle_t *s, mulib_strbuf_t *out) {
  if (!s || !out) return 0;
  return !s->enabled || (s->info && mulib_terminfo_emit(s->info,"sgr0",NULL,out));
}
int mulib_termstyle_apply(const mulib_termstyle_t *s, const mulib_termstyle_spec_t *spec, mulib_strbuf_t *out) {
  static const char *const caps[] = {"bold","dim","sitm","smul","rev"};
  mulib_strbuf_t tmp = {NULL,0,0};
  int32_t params[9] = {0}, colors;
  unsigned i;
  int ok = 0;
  if (!s || !spec || !out || (spec->attributes & ~31u) || spec->foreground < -1 || spec->background < -1) return 0;
  if (!s->enabled) return 1;
  if (!s->info || !mulib_termstyle_reset(s,&tmp)) goto done;
  for (i = 0; i < 5; ++i)
    if ((spec->attributes & (1u << i)) && !mulib_terminfo_emit(s->info,caps[i],NULL,&tmp)) goto done;
  colors = mulib_terminfo_num(s->info,"colors");
  if (spec->foreground >= 0) {
    params[0] = spec->foreground;
    if (params[0] >= colors || !mulib_terminfo_emit(s->info,"setaf",params,&tmp)) goto done;
  }
  if (spec->background >= 0) {
    params[0] = spec->background;
    if (params[0] >= colors || !mulib_terminfo_emit(s->info,"setab",params,&tmp)) goto done;
  }
  ok = mulib_strbuf_append_n(out,tmp.data,tmp.length);
done:
  mulib_strbuf_free(&tmp); return ok;
}
int mulib_termstyle_text(const mulib_termstyle_t *s, const mulib_termstyle_spec_t *spec,
                         const char *text, size_t n, mulib_strbuf_t *out) {
  mulib_strbuf_t tmp = {NULL,0,0};
  int ok;
  if (!out || (!text && n)) return 0;
  ok = mulib_termstyle_apply(s,spec,&tmp) && mulib_strbuf_append_n(&tmp,text,n)
      && mulib_termstyle_reset(s,&tmp) && mulib_strbuf_append_n(out,tmp.data,tmp.length);
  mulib_strbuf_free(&tmp); return ok;
}
static int mulib_termstyle_error_(mulib_xml_error_t *e, int code, const char *message) {
  if (e) { e->offset = 0; e->message = message; }
  return code;
}
static int mulib_termstyle_lower_(const char *s, char *out, size_t cap) {
  size_t n, i;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' || *s == '\f') ++s;
  n = strlen(s);
  while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == '\f')) --n;
  if (n >= cap) return 0;
  for (i = 0; i < n; ++i) out[i] = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] + 32) : s[i];
  out[n] = 0; return 1;
}
static int mulib_termstyle_color_(const char *s, uint32_t *out) {
  static const struct { const char *name; uint32_t rgb; } colors[] = {
    {"black",0x000000}, {"silver",0xc0c0c0}, {"gray",0x808080}, {"white",0xffffff},
    {"maroon",0x800000}, {"red",0xff0000}, {"purple",0x800080}, {"fuchsia",0xff00ff},
    {"green",0x008000}, {"lime",0x00ff00}, {"olive",0x808000}, {"yellow",0xffff00},
    {"navy",0x000080}, {"blue",0x0000ff}, {"teal",0x008080}, {"aqua",0x00ffff},
    {"orange",0xffa500}, {"rebeccapurple",0x663399}
  };
  size_t i, n = strlen(s);
  uint32_t rgb = 0;
  if (!strcmp(s, "grey")) s = "gray";
  for (i = 0; i < sizeof colors / sizeof colors[0]; ++i)
    if (!strcmp(s, colors[i].name)) { *out = UINT32_C(0x01000000) | colors[i].rgb; return 1; }
  if (*s == '#' && (n == 4 || n == 7)) {
    for (i = 1; i < n; ++i) {
      unsigned d;
      if (s[i] >= '0' && s[i] <= '9') d = (unsigned)(s[i] - '0');
      else if (s[i] >= 'a' && s[i] <= 'f') d = (unsigned)(s[i] - 'a' + 10);
      else return 0;
      rgb = n == 4 ? (rgb << 8) | d * 17 : (rgb << 4) | d;
    }
    *out = UINT32_C(0x01000000) | rgb; return 1;
  }
  if (!strncmp(s, "rgb(", 4)) {
    s += 4;
    for (i = 0; i < 3; ++i) {
      unsigned value = 0, digits = 0;
      while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f') ++s;
      while (*s >= '0' && *s <= '9') {
        value = value * 10 + (unsigned)(*s++ - '0'); ++digits;
        if (value > 255) return 0;
      }
      if (!digits) return 0;
      while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f') ++s;
      if (*s++ != (i == 2 ? ')' : ',')) return 0;
      rgb = (rgb << 8) | value;
    }
    if (*s) return 0;
    *out = UINT32_C(0x01000000) | rgb; return 1;
  }
  return 0;
}
static int mulib_termstyle_valid_(mulib_term_style_t style) {
  mulib_term_cell_t c;
  c.codepoint = ' '; c.style = style; return mulib_term_cell_valid(c);
}
static const char *const mulib_termstyle_properties_[] = {
  "color", "background-color", "font-weight", "font-style", "text-decoration",
  "visibility", "--term-inverse", "--term-dim", "--term-blink"
};
static int mulib_termstyle_property_(mulib_term_style_t *s, mulib_term_style_t parent,
                                      size_t p, const char *text) {
  char v[128];
  unsigned bits = 0, mask;
  uint32_t color;
  int inherit, initial;
  if (!mulib_termstyle_lower_(text, v, sizeof v)) return 0;
  inherit = !strcmp(v, "inherit") || (!strcmp(v, "unset") && p != 1);
  initial = !strcmp(v, "initial") || (!strcmp(v, "unset") && p == 1);
  if (p < 2) {
    if (inherit) color = p ? parent.background : parent.foreground;
    else if (initial) color = MULIB_TERM_COLOR_DEFAULT;
    else if (!strcmp(v, "currentcolor")) color = p ? s->foreground : parent.foreground;
    else if (p && !strcmp(v, "transparent")) color = parent.background;
    else if (!mulib_termstyle_color_(v, &color)) return 0;
    if (p) s->background = color; else s->foreground = color;
    return 1;
  }
  mask = p == 2 ? MULIB_TERM_BOLD : p == 3 ? MULIB_TERM_ITALIC
       : p == 4 ? MULIB_TERM_UNDERLINE | MULIB_TERM_STRIKE | MULIB_TERM_BLINK
       : p == 5 ? MULIB_TERM_CONCEAL : p == 6 ? MULIB_TERM_INVERSE
       : p == 7 ? MULIB_TERM_DIM : MULIB_TERM_BLINK;
  if (inherit) bits = parent.attributes & mask;
  else if (initial) bits = 0;
  else if (p == 2) {
    if (!strcmp(v, "bold") || (strlen(v) == 3 && v[0] >= '6' && v[0] <= '9' && v[1] == '0' && v[2] == '0')) bits = mask;
    else if (strcmp(v, "normal") && !(strlen(v) == 3 && v[0] >= '1' && v[0] <= '5' && v[1] == '0' && v[2] == '0')) return 0;
  } else if (p == 3) {
    if (!strcmp(v, "italic") || !strcmp(v, "oblique")) bits = mask;
    else if (strcmp(v, "normal")) return 0;
  } else if (p == 4) {
    char *token = v;
    if (strcmp(v, "none")) while (*token) {
      char *end = token;
      while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n' && *end != '\f') ++end;
      if (*end) *end++ = 0;
      if (!strcmp(token, "underline")) bits |= MULIB_TERM_UNDERLINE;
      else if (!strcmp(token, "line-through")) bits |= MULIB_TERM_STRIKE;
      else if (!strcmp(token, "blink")) bits |= MULIB_TERM_BLINK;
      else return 0;
      while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' || *end == '\f') ++end;
      token = end;
    }
    if (!*v) return 0;
  } else if (p == 5) {
    if (!strcmp(v, "hidden")) bits = mask; else if (strcmp(v, "visible")) return 0;
  } else {
    if (!strcmp(v, "on")) bits = mask; else if (strcmp(v, "off")) return 0;
  }
  s->attributes = (s->attributes & ~mask) | bits; return 1;
}
int mulib_termstyle_from_css(const mulib_css_style_t *d, const mulib_term_style_t *parent,
                          mulib_term_style_t *out, mulib_xml_error_t *e) {
  mulib_term_style_t base = parent ? *parent : mulib_term_style_default(), s = base;
  size_t i, p;
  const char *v;
  if (e) { e->offset = 0; e->message = NULL; }
  if (!out || !mulib_termstyle_valid_(base)) return mulib_termstyle_error_(e, MULIB_TERMSTYLE_EINVAL, "invalid parent/output");
  for (i = 0; i < mulib_css_count(d); ++i) {
    char name[64];
    const char *raw = mulib_css_name(d, i);
    if (!mulib_termstyle_lower_(raw, name, sizeof name)) return mulib_termstyle_error_(e, MULIB_TERMSTYLE_ENOTSUP, "unsupported terminal property");
    for (p = 0; p < 9; ++p) if (!strcmp(name, mulib_termstyle_properties_[p])) break;
    /* Custom property names are case-sensitive, like mulib-css. */
    if (p == 9 || (p >= 6 && strcmp(raw, name))) return mulib_termstyle_error_(e, MULIB_TERMSTYLE_ENOTSUP, "unsupported terminal property");
  }
  for (p = 0; p < 9; ++p) {
    v = mulib_css_get(d, mulib_termstyle_properties_[p]);
    if (v && !mulib_termstyle_property_(&s, base, p, v)) return mulib_termstyle_error_(e, MULIB_TERMSTYLE_ENOTSUP, "unsupported terminal value");
  }
  *out = s; return 0;
}
int mulib_termstyle_resolve(const mulib_css_sheet_t *sheet, const mulib_xml_node_t *node,
                            const mulib_term_style_t *parent, mulib_term_style_t *out, mulib_xml_error_t *e) {
  const char *text;
  mulib_css_style_t *in = NULL, *d;
  int result;
  if (e) { e->offset = 0; e->message = NULL; }
  if (!node || !out || mulib_xml_type(node) != MULIB_XML_ELEMENT)
    return mulib_termstyle_error_(e, MULIB_TERMSTYLE_EINVAL, "invalid terminal element/output");
  text = mulib_xml_attr(node, "style");
  if (text && !(in = mulib_css_parse_declarations(text, strlen(text), e))) return MULIB_TERMSTYLE_EINVAL;
  d = mulib_css_cascade(sheet, node, in, e); mulib_css_style_destroy(in);
  if (!d) return MULIB_TERMSTYLE_ENOMEM;
  result = mulib_termstyle_from_css(d, parent, out, e); mulib_css_style_destroy(d); return result;
}
static uint32_t mulib_termstyle_palette_(unsigned i) {
  static const uint32_t basic[16] = {
    0x000000,0x800000,0x008000,0x808000,0x000080,0x800080,0x008080,0xc0c0c0,
    0x808080,0xff0000,0x00ff00,0xffff00,0x0000ff,0xff00ff,0x00ffff,0xffffff
  };
  static const unsigned cube[6] = {0,95,135,175,215,255};
  if (i < 16) return basic[i];
  if (i < 232) { i -= 16; return (cube[i / 36] << 16) | (cube[i / 6 % 6] << 8) | cube[i % 6]; }
  i = 8 + (i - 232) * 10; return (i << 16) | (i << 8) | i;
}
static unsigned mulib_termstyle_nearest_(uint32_t rgb, unsigned count) {
  unsigned i, best = 0, distance = UINT32_MAX;
  for (i = 0; i < count; ++i) {
    uint32_t c = mulib_termstyle_palette_(i);
    int r = (int)((rgb >> 16) & 255) - (int)((c >> 16) & 255);
    int g = (int)((rgb >> 8) & 255) - (int)((c >> 8) & 255);
    int b = (int)(rgb & 255) - (int)(c & 255);
    unsigned d = (unsigned)(r*r + g*g + b*b);
    if (d < distance) { distance = d; best = i; }
  }
  return best;
}
static int mulib_termstyle_emit_(mulib_term_style_t s, const mulib_terminfo_t *info, unsigned flags, mulib_strbuf_t *out) {
  static const char *const attrs[8] = {"bold","dim","sitm","smul","blink","rev","invis",NULL};
  unsigned i;
  int32_t params[9] = {0}, colors;
  if (!info || (flags & MULIB_TERMSTYLE_PLAIN)) return 0;
  if ((flags & MULIB_TERMSTYLE_TRUECOLOR) && !info->ansi) return MULIB_TERMSTYLE_ENOTSUP;
  if (!mulib_terminfo_emit(info, "sgr0", NULL, out) || !mulib_terminfo_emit(info, "op", NULL, out)) return MULIB_TERMSTYLE_ENOTSUP;
  for (i = 0; i < 8; ++i) if (s.attributes & (1u << i)) {
    if (i == 7) {
      if (!info->ansi) return MULIB_TERMSTYLE_ENOTSUP;
      if (!mulib_strbuf_append(out, "\033[9m")) return MULIB_TERMSTYLE_ENOMEM;
    } else if (!mulib_terminfo_emit(info, attrs[i], NULL, out)) return MULIB_TERMSTYLE_ENOTSUP;
  }
  colors = mulib_terminfo_num(info, "colors");
  for (i = 0; i < 2; ++i) {
    uint32_t c = i ? s.background : s.foreground;
    unsigned count = colors >= 256 ? 256u : colors >= 16 ? 16u : colors >= 8 ? 8u : 0u;
    if (c == MULIB_TERM_COLOR_DEFAULT) continue;
    if ((c >> 24) == 1 && (flags & MULIB_TERMSTYLE_TRUECOLOR)) {
      char text[48];
      int n = snprintf(text, sizeof text, "\033[%u;2;%u;%u;%um", i ? 48u : 38u,
                        (unsigned)((c >> 16) & 255), (unsigned)((c >> 8) & 255), (unsigned)(c & 255));
      if (n < 0 || !mulib_strbuf_append_n(out, text, (size_t)n)) return MULIB_TERMSTYLE_ENOMEM;
    } else {
      if (!count) return MULIB_TERMSTYLE_ENOTSUP;
      if ((c >> 24) == 1) params[0] = (int32_t)mulib_termstyle_nearest_(c, count);
      else params[0] = c < count ? (int32_t)c : (int32_t)mulib_termstyle_nearest_(mulib_termstyle_palette_(c), count);
      if (!mulib_terminfo_emit(info, i ? "setab" : "setaf", params, out)) return MULIB_TERMSTYLE_ENOTSUP;
    }
  }
  return 0;
}
int mulib_termstyle_emit(mulib_term_style_t s, const mulib_terminfo_t *info, unsigned flags, mulib_strbuf_t *out) {
  mulib_strbuf_t tmp;
  int result;
  if (!out || (flags & ~3u) || !mulib_termstyle_valid_(s)) return MULIB_TERMSTYLE_EINVAL;
  mulib_strbuf_init(&tmp); result = mulib_termstyle_emit_(s, info, flags, &tmp);
  if (!result && !mulib_strbuf_append_n(out, tmp.data, tmp.length)) result = MULIB_TERMSTYLE_ENOMEM;
  mulib_strbuf_free(&tmp); return result;
}
int mulib_termstyle_write(mulib_term_style_t s, const mulib_terminfo_t *info, unsigned flags,
                         const char *text, size_t n, mulib_strbuf_t *out) {
  mulib_strbuf_t tmp;
  size_t at = 0, used;
  uint32_t cp;
  int result;
  if (!out || (!text && n) || (flags & ~3u) || !mulib_termstyle_valid_(s)
      || !mulib_utf8_validate((const unsigned char *)text, n)) return MULIB_TERMSTYLE_EINVAL;
  mulib_strbuf_init(&tmp); result = mulib_termstyle_emit_(s, info, flags, &tmp);
  while (!result && at < n) {
    (void)mulib_utf8_decode((const unsigned char *)text + at, n - at, &cp, &used);
    if ((cp < 32 && cp != '\n' && cp != '\t') || (cp >= 127 && cp < 160)) {
      if (!mulib_strbuf_append_n(&tmp, "\xef\xbf\xbd", 3)) result = MULIB_TERMSTYLE_ENOMEM;
    } else if (!mulib_strbuf_append_n(&tmp, text + at, used)) result = MULIB_TERMSTYLE_ENOMEM;
    at += used;
  }
  if (!result) result = mulib_termstyle_emit_(mulib_term_style_default(), info, flags, &tmp);
  if (!result && !mulib_strbuf_append_n(out, tmp.data, tmp.length)) result = MULIB_TERMSTYLE_ENOMEM;
  mulib_strbuf_free(&tmp); return result;
}
#ifdef __cplusplus
}
#endif
#endif
