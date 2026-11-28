#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DOM
#define MULIB_IMPL_HTML_DOMBRIDGE_NATIVE
#include "mulib-html-dombridge.h"

#include <stdio.h>

int
main (void)
{
  static const char input[] = "<title>Offline demo</title><p id=message>Hello &amp; goodbye";
  const mulib_html_dombridge_limits_t limits = { 128, 16, 8 };
  mulib_dom_document_t *document = NULL;
  if (mulib_html_dombridge_parse (input, sizeof input - 1, &limits, &document)
      != MULIB_HTML_DOMBRIDGE_OK)
    return 1;

  mulib_dom_node_t *p;
  mulib_dom_error_t err;
  int ok = mulib_dom_query_native_first (mulib_dom_document_node (document),
                                         "#message", 16, &p, &err) && p;
  mulib_buffer_t utf8 = { 0 };
  if (ok)
    ok = mulib_dom_string_to_utf8 (
        mulib_dom_node_value (mulib_dom_first_child (p)), &utf8);
  if (ok && utf8.length)
    ok = fwrite (utf8.data, 1, utf8.length, stdout) == utf8.length
         && putchar ('\n') != EOF;
  mulib_buffer_free (&utf8);
  mulib_dom_document_destroy (document);
  return !ok;
}
