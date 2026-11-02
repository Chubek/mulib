/* Compile as a freestanding, position-independent relocatable object:
 * cc -std=c11 -fPIC -fno-builtin -fno-stack-protector
 *    -fno-asynchronous-unwind-tables -Iinclude -c examples/plugins/metapp-count.c
 *    -o metapp-count.o
 * Load with the metapp example's --plugin=metapp-count.o option.
 */
#include "mulib-metapp.h"
static unsigned counter;
static int equal (const char *a, const char *b) {
  while (*a && *a == *b) { ++a; ++b; }
  return *a == *b;
}
static size_t length (const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
static int handle (void *user, const mulib_metapp_host_t *host, mulib_metapp_t *pp,
                   const mulib_metapp_event_t *event, mulib_strbuf_t *out) {
  unsigned *value = (unsigned *)user;
  if (event->kind == MULIB_METAPP_DIRECTIVE && equal (event->name, "metapp_emit"))
    return host->append (out, event->arguments, length (event->arguments)) ? 1 : -1;
  if (event->kind == MULIB_METAPP_DIRECTIVE && equal (event->name, "metapp_define"))
    return host->define (pp, event->arguments, NULL) ? -1 : 1;
  if (event->kind == MULIB_METAPP_PRAGMA && equal (event->name, "metapp_note")) return 1;
  if (event->kind == MULIB_METAPP_IDENTIFIER && equal (event->name, "__PLUGIN_COUNT__")) {
    char text[16]; size_t n = 0, i; unsigned x = (*value)++;
    do { text[n++] = (char)('0'+x%10); x /= 10; } while (x);
    for (i = 0; i < n/2; ++i) { char c = text[i]; text[i] = text[n-i-1]; text[n-i-1] = c; }
    return host->append (out, text, n) ? 1 : -1;
  }
  return 0;
}
const mulib_metapp_plugin_t mulib_metapp_plugin_v1 = {
  MULIB_METAPP_PLUGIN_ABI, sizeof (mulib_metapp_plugin_t), "counter", handle, &counter, NULL
};
