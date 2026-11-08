#define MULIB_IMPL_DOM
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_UTF8
#include "mulib-dom.h"
#include <assert.h>

static void
changed (mulib_dom_event_t *event, void *userdata)
{
  int *count = (int *)userdata;
  assert (mulib_dom_event_phase (event) == 3);
  ++*count;
}

int
main (void)
{
  mulib_dom_document_t *document = mulib_dom_document_new ();
  mulib_dom_node_t *root, *text;
  mulib_dom_event_t *event;
  mulib_dom_error_t error;
  mulib_buffer_t content = { 0 }, utf8 = { 0 };
  mulib_dom_string_t value;
  int calls = 0;
  assert (document);
  root = mulib_dom_create_element (document, NULL, "message", &error);
  text = mulib_dom_create_text_utf8 (document, "Hello, DOM!", 11, &error);
  assert (root && text);
  assert (mulib_dom_append_child (mulib_dom_document_node (document), root,
                                  &error));
  assert (mulib_dom_append_child (root, text, &error));
  assert (mulib_dom_add_event_listener (root, "change", changed, &calls, NULL,
                                        &error));
  event = mulib_dom_event_new ("change", 1, 0, NULL);
  assert (event && mulib_dom_dispatch_event (text, event, &error) == 1);
  assert (calls == 1 && mulib_dom_event_destroy (event));
  assert (mulib_dom_text_content (root, &content, &error));
  value.data = (const uint16_t *)content.data;
  value.length = content.length / sizeof (uint16_t);
  assert (mulib_dom_string_to_utf8 (value, &utf8));
  assert (fwrite (utf8.data, 1, utf8.length, stdout) == utf8.length);
  assert (putchar ('\n') != EOF);
  mulib_buffer_free (&content);
  mulib_buffer_free (&utf8);
  assert (mulib_dom_document_destroy (document));
  return 0;
}
