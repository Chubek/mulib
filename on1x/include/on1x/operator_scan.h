#ifndef ON1X_OPERATOR_SCAN_H
#define ON1X_OPERATOR_SCAN_H

#include <stddef.h>
#include <string.h>

/* One recognizer for both scanner profiles and parser spelling validation.
 * Structural punctuation (including colon, dot and arrows) is not part of
 * the ordinary operator alphabet. '$' and '-' may occur inside <...>.
 * A plain single character or exact builtin goes to the ordinary scanner. */
/* Scanning a longer run as one candidate gives a single located error, and
 * also keeps repeated closing angles available to the type parser. */
enum { ON1X_OPERATOR_MAX_BYTES = 32, ON1X_OPERATOR_SPELLING_MAX_BYTES = 6 };

static inline int on1x_operator_glyph(char c) {
  return c && strchr("!%&*+?^|~", c) != NULL;
}

static inline int on1x_operator_builtin_pair(char a, char b) {
  return (b == '=' && (a == '!' || a == '<' || a == '>' || a == '=')) ||
         (b == '>' && (a == '=' || a == '-')) || (a == '<' && b == '-') ||
         (a == b && (a == '&' || a == '|'));
}

static inline size_t on1x_operator_width(const char *s, size_t n) {
  if (n < 2) return 0;
  /* These forms would otherwise start with an exact builtin. */
  if (n >= 3 && (memcmp(s, ">=>", 3) == 0 || memcmp(s, "<=<", 3) == 0 ||
                 memcmp(s, "=<<", 3) == 0)) return 3;
  if ((s[0] == '<' || s[0] == '>') && s[1] == s[0]) {
    size_t i = 2;
    while (i < n && i < ON1X_OPERATOR_MAX_BYTES && s[i] == s[0]) ++i;
    if (i < n && i < ON1X_OPERATOR_MAX_BYTES &&
        (s[i] == '=' || (i == 2 && s[i] == '.' && (i + 1 == n || s[i + 1] != '.')))) ++i;
    return i;
  }
  if (s[0] == '<') {
    size_t i = 1;
    while (i < n && i < ON1X_OPERATOR_MAX_BYTES &&
           (on1x_operator_glyph(s[i]) || s[i] == '$' || s[i] == '-')) ++i;
    if (i > 1 && i < n && i < ON1X_OPERATOR_MAX_BYTES && s[i] == '>') return i + 1;
    if (s[1] == '|') return 2;
    return 0;
  }
  if (!on1x_operator_glyph(s[0]) || !on1x_operator_glyph(s[1]) ||
      on1x_operator_builtin_pair(s[0], s[1])) {
    if ((s[0] == '|' || s[0] == '~' || s[0] == '!') && s[1] == '>') return 2;
    return 0;
  }
  /* Keep traditional adjacent prefix ! and postfix ? boundaries, except
   * homogeneous runs and the explicit pairing spellings |? and ?|. */
  if ((s[0] == '?' && s[1] != '?' && s[1] != '|') ||
      (s[1] == '!' && s[0] != '!') ||
      (s[1] == '?' && s[0] != '?' && s[0] != '|')) return 0;
  size_t i = 2;
  while (i < n && i < ON1X_OPERATOR_MAX_BYTES && on1x_operator_glyph(s[i]) &&
         !on1x_operator_builtin_pair(s[i - 1], s[i]) &&
         !on1x_operator_builtin_pair(s[i], i + 1 < n ? s[i + 1] : 0) &&
         (s[i] != '!' || s[i - 1] == '!') &&
         (s[i] != '?' || s[i - 1] == '?' || s[i - 1] == '|') &&
         (s[i - 1] != '?' || s[i] == '?' || s[i] == '|')) ++i;
  if (on1x_operator_builtin_pair(s[1], n > 2 ? s[2] : 0)) return 0;
  return i;
}

static inline int on1x_operator_spelling(const char *s, size_t n) {
  return n >= 2 && n <= ON1X_OPERATOR_SPELLING_MAX_BYTES &&
         on1x_operator_width(s, n) == n;
}

#endif
