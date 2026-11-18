#ifndef ON1X_REGEX_SCAN_H
#define ON1X_REGEX_SCAN_H
#include <stddef.h>

/* Return the byte width of a same-line slash literal, or zero for an
 * unfinished literal. Slash in a class still requires escaping. */
static inline size_t on1x_regex_literal_width(const char *s, size_t n) {
 size_t i;
 for (i=1; i<n && s[i]!='\n' && s[i]!='\r'; ++i) {
  if (s[i]=='/') return i+1;
  if (s[i]=='\\') {
   if (++i==n || s[i]=='\n' || s[i]=='\r') return 0;
  }
 }
 return 0;
}
#endif
