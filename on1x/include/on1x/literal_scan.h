#ifndef ON1X_LITERAL_SCAN_H
#define ON1X_LITERAL_SCAN_H

/* Shared bounded complex-terminal recognizer for the generated and bootstrap
 * scanners. The parser owns decoded values; scanners retain exact source. */
#include <stddef.h>
#include <string.h>

static inline int on1x_lit_ident_start(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline int on1x_lit_ident(unsigned char c) {
  return on1x_lit_ident_start(c) || (c >= '0' && c <= '9');
}
static inline int on1x_lit_hex(unsigned char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static inline size_t on1x_lit_character(const char *s, size_t n) {
  size_t pos = 2;
  unsigned char c;
  if (pos == n || strchr(" \t\r\n", s[pos])) return 0;
  c = (unsigned char)s[pos++];
  if (c >= 128) {
    while (pos < n && ((unsigned char)s[pos] & 0xc0) == 0x80) ++pos;
  } else if (on1x_lit_ident(c)) {
    if ((c == 'u' || c == 'U') && pos < n && s[pos] == '+') ++pos;
    while (pos < n && on1x_lit_ident((unsigned char)s[pos])) ++pos;
  }
  return pos;
}

static inline size_t on1x_lit_string(const char *, size_t, unsigned);

/* Return the offset of a matching closing brace (or n on error). Braces in
 * nested tokens are not delimiters. Actual expression parsing is deferred to
 * the canonical parser. */
static inline size_t on1x_lit_interpolation(const char *s, size_t n, unsigned depth) {
  size_t pos = 0, braces = 1;
  if (depth >= 32) return n;
  while (pos < n) {
    char c = s[pos];
    if (c == '{') { if (++braces > 256) return n; ++pos; }
    else if (c == '}') { if (!--braces) return pos; ++pos; }
    else if (c == '#') {
      if (pos + 1 < n && s[pos + 1] == '\\') {
        size_t len = on1x_lit_character(s + pos, n - pos);
        if (!len) return n;
        pos += len;
      } else {
        while (pos < n && s[pos] != '\n') ++pos;
      }
    } else if (c == '"' || (c == '`') ||
               ((c == 'b' || c == 'r') && pos + 2 < n && s[pos + 1] == '!' && s[pos + 2] == '"')) {
      size_t len;
      if (c == '`') {
        size_t delim = n - pos >= 3 && s[pos + 1] == '`' && s[pos + 2] == '`' ? 3 : 1;
        size_t i = pos + delim;
        while (i < n && !(s[i] == '`' && (delim == 1 ||
               (n - i >= 3 && s[i + 1] == '`' && s[i + 2] == '`')))) {
          if (delim == 1 && (s[i] == '\r' || s[i] == '\n')) return n;
          if (delim == 1 && s[i] == '\\') {
            ++i;
            if (i == n || s[i] == '\r' || s[i] == '\n') return n;
          }
          ++i;
        }
        if (i == n) return n;
        len = i + delim - pos;
      } else len = on1x_lit_string(s + pos, n - pos, depth + 1);
      if (!len) return n;
      pos += len;
    } else if (on1x_lit_ident((unsigned char)c)) {
      do { ++pos; } while (pos < n && on1x_lit_ident((unsigned char)s[pos]));
    } else ++pos;
  }
  return n;
}

/* s begins at ", r!", or b!". Raw strings don't process escapes or $. */
static inline size_t on1x_lit_string(const char *s, size_t n, unsigned depth) {
  size_t prefix = s[0] == '"' ? 0 : 2, pos, delim;
  int raw = s[0] == 'r', bytes = s[0] == 'b';
  delim = n - prefix >= 3 && s[prefix + 1] == '"' && s[prefix + 2] == '"' ? 3 : 1;
  pos = prefix + delim;
  while (pos < n) {
    char c = s[pos++];
    if (c == '"' && (delim == 1 || (n - pos >= 2 && s[pos] == '"' && s[pos + 1] == '"')))
      return pos + delim - 1;
    if (delim == 1 && (c == '\n' || c == '\r')) return 0;
    if (!raw && c == '\\') {
      if (pos == n) return 0;
      c = s[pos++];
      if (bytes && c == 'x') {
        if (n - pos < 2 || on1x_lit_hex((unsigned char)s[pos]) < 0 ||
            on1x_lit_hex((unsigned char)s[pos + 1]) < 0) return 0;
        pos += 2;
      } else if (!strchr("\\\"nrt0", c) && (bytes || c != '$')) return 0;
    } else if (!raw && !bytes && c == '$' && pos < n && s[pos] == '{') {
      size_t remaining = n - ++pos;
      size_t close = on1x_lit_interpolation(s + pos, remaining, depth);
      if (close == remaining) return 0;
      pos += close + 1;
    } else if (!raw && !bytes && c == '$' && pos < n && s[pos] == '$') ++pos;
  }
  return 0;
}
#endif
