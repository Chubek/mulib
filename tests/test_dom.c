#include <assert.h>
#include <stdlib.h>
#include <string.h>
static size_t allocations, fail_at;
static void *
dom_malloc (size_t n)
{
  return ++allocations == fail_at ? NULL : malloc (n);
}
static void *
dom_calloc (size_t n, size_t s)
{
  return ++allocations == fail_at ? NULL : calloc (n, s);
}
static void *
dom_realloc (void *p, size_t n)
{
  return ++allocations == fail_at ? NULL : realloc (p, n);
}
#define malloc dom_malloc
#define calloc dom_calloc
#define realloc dom_realloc
#define MULIB_IMPL_DOM
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_UTF8
#include "mulib-dom.h"
#undef malloc
#undef calloc
#undef realloc

static mulib_dom_string_t
span (const uint16_t *s, size_t n)
{
  mulib_dom_string_t v = { s, n };
  return v;
}
static mulib_dom_node_t *
element (mulib_dom_document_t *d, const char *name)
{
  mulib_dom_node_t *n = mulib_dom_create_element (d, NULL, name, NULL);
  assert (n);
  return n;
}
static mulib_dom_node_t *
text (mulib_dom_document_t *d, const char *s)
{
  mulib_dom_node_t *n = mulib_dom_create_text_utf8 (d, s, strlen (s), NULL);
  assert (n);
  return n;
}
static void
assert_text (const mulib_dom_node_t *n, const char *expected)
{
  mulib_buffer_t b = { 0 }, utf8 = { 0 };
  assert (mulib_dom_text_content (n, &b, NULL));
  assert (mulib_dom_string_to_utf8 (
      span ((const uint16_t *)b.data, b.length / 2), &utf8));
  assert (utf8.length == strlen (expected));
  assert (!utf8.length || !memcmp (utf8.data, expected, utf8.length));
  mulib_buffer_free (&b);
  mulib_buffer_free (&utf8);
}
static void
trees (void)
{
  mulib_dom_document_t *d = mulib_dom_document_new (),
                       *other = mulib_dom_document_new ();
  mulib_dom_node_t *doc = mulib_dom_document_node (d),
                   *root = element (d, "root");
  mulib_dom_node_t *a = element (d, "a"), *b = element (d, "b"), *f, *dt,
                   *clone;
  mulib_dom_error_t e;
  assert (d && other && !mulib_dom_owner_document (doc));
  assert (mulib_dom_append_child (doc, root, &e) == root && !e);
  assert (!mulib_dom_append_child (doc, a, &e)
          && e == MULIB_DOM_HIERARCHY_REQUEST_ERR);
  assert (!mulib_dom_append_child (root, doc, &e));
  assert (mulib_dom_append_child (root, a, &e));
  assert (mulib_dom_append_child (root, b, &e));
  assert (mulib_dom_insert_before (root, b, a, &e));
  assert (mulib_dom_first_child (root) == b
          && mulib_dom_last_child (root) == a);
  assert (mulib_dom_insert_before (root, b, b, &e) == b);
  assert (!mulib_dom_insert_before (a, b, b, &e)
          && e == MULIB_DOM_NOT_FOUND_ERR);
  assert (!mulib_dom_append_child (a, root, &e));
  assert (mulib_dom_parent (a) == root && mulib_dom_child_count (root) == 2);
  assert (mulib_dom_child_at (root, 1) == a
          && !mulib_dom_child_at (root, SIZE_MAX));
  assert (mulib_dom_compare_document_position (root, a) == 20);
  assert (mulib_dom_compare_document_position (a, root) == 10);
  assert (mulib_dom_compare_document_position (b, a) == 4);
  assert (mulib_dom_compare_document_position (a, b) == 2);
  dt = mulib_dom_create_doctype (d, "root", "pub", "sys", &e);
  assert (dt && !mulib_dom_append_child (doc, dt, &e));
  assert (mulib_dom_insert_before (doc, dt, root, &e));
  assert (!strcmp (mulib_dom_public_id (dt), "pub"));
  assert (!strcmp (mulib_dom_system_id (dt), "sys"));
  assert (!mulib_dom_insert_before (doc, root, dt, &e));
  f = mulib_dom_create_fragment (d, &e);
  assert (f && mulib_dom_append_child (f, a, &e));
  assert (mulib_dom_append_child (f, b, &e));
  assert (!mulib_dom_replace_child (doc, f, root, &e));
  assert (mulib_dom_child_count (f) == 2
          && mulib_dom_document_element (d) == root);
  assert (mulib_dom_remove_child (f, b, &e) == b);
  assert (mulib_dom_replace_child (doc, f, root, &e) == root);
  assert (!mulib_dom_first_child (f) && mulib_dom_document_element (d) == a);
  assert (!mulib_dom_is_connected (root) && mulib_dom_is_connected (a));
  assert (mulib_dom_append_child (a, b, &e));
  assert (mulib_dom_append_child (b, text (d, "hello"), &e));
  clone = mulib_dom_import_node (other, a, 1, &e);
  assert (clone && mulib_dom_is_equal_node (clone, a));
  assert (mulib_dom_owner_document (clone) == other);
  assert (!mulib_dom_parent (clone));
  assert (mulib_dom_adopt_node (other, a, &e) == a);
  assert (!mulib_dom_document_element (d)
          && mulib_dom_owner_document (b) == other);
  assert (mulib_dom_document_destroy (d));
  assert_text (a, "hello");
  assert (mulib_dom_append_child (mulib_dom_document_node (other), a, &e));
  assert (
      !mulib_dom_import_node (other, mulib_dom_document_node (other), 1, &e));
  assert (!mulib_dom_adopt_node (other, mulib_dom_document_node (other), &e));
  assert (mulib_dom_document_destroy (other));
}
static void
strings_and_attributes (void)
{
  const uint16_t id[] = { 'k', 'e', 'y' },
                 units[] = { 'A', 0xd83d, 0xde00, 0, 'B' };
  mulib_dom_document_t *d = mulib_dom_document_new (),
                       *other = mulib_dom_document_new ();
  mulib_dom_node_t *root = element (d, "root"), *a, *n, *tail, *empty, *clone;
  mulib_buffer_t b = { 0 };
  mulib_dom_error_t e;
  assert (!mulib_dom_create_element (d, NULL, "p:x", &e)
          && e == MULIB_DOM_NAMESPACE_ERR);
  assert (!mulib_dom_create_element (d, "urn:x", "xml:x", &e));
  assert (!mulib_dom_create_element (d, NULL, "a:b:c", &e));
  assert (!mulib_dom_create_element (d, NULL, "1x", &e));
  assert (mulib_dom_append_child (mulib_dom_document_node (d), root, &e));
  assert (mulib_dom_set_attribute (root, NULL, "id", span (id, 3), &e));
  a = mulib_dom_get_attribute_node (root, NULL, "id");
  assert (a && mulib_dom_attribute_at (root, 0) == a
          && mulib_dom_attribute_count (root) == 1);
  assert (mulib_dom_owner_element (a) == root && !mulib_dom_parent (a));
  assert (mulib_dom_compare_document_position (root, a) == 20);
  assert (mulib_dom_compare_document_position (a, root) == 10);
  assert (
      mulib_dom_get_element_by_id (mulib_dom_document_node (d), span (id, 3))
      == root);
  assert (!mulib_dom_get_element_by_id (root, span (id, 3)));
  assert (mulib_dom_set_attribute (root, NULL, "id", span (id, 3), &e));
  assert (mulib_dom_get_attribute_node (root, NULL, "id") == a);
  n = element (d, "child");
  assert (!mulib_dom_set_attribute_node (n, a, &e)
          && e == MULIB_DOM_INUSE_ATTRIBUTE_ERR);
  assert (mulib_dom_adopt_node (other, a, &e));
  assert (!mulib_dom_attribute_count (root) && !mulib_dom_owner_element (a));
  assert (!mulib_dom_set_attribute_node (root, a, &e)
          && e == MULIB_DOM_WRONG_DOCUMENT_ERR);
  assert (mulib_dom_adopt_node (d, a, &e)
          && mulib_dom_set_attribute_node (root, a, &e));
  assert (mulib_dom_append_child (root, n, &e));
  clone = mulib_dom_clone_node (root, 1, &e);
  assert (clone && mulib_dom_is_equal_node (clone, root));
  assert (!mulib_dom_append_child (root, a, &e));
  assert (mulib_dom_get_elements_by_tag_name (mulib_dom_document_node (d), "*",
                                              "*", &b, &e));
  assert (b.length == 2 * sizeof (mulib_dom_node_t *));
  assert (((mulib_dom_node_t **)b.data)[0] == root
          && ((mulib_dom_node_t **)b.data)[1] == n);
  mulib_buffer_free (&b);
  assert (mulib_dom_set_attribute (root, MULIB_DOM_XMLNS_NAMESPACE, "xmlns:p",
                                   span (id, 3), &e));
  assert (mulib_dom_lookup_namespace_uri (n, "p", &b, &e) == 1);
  assert (b.length == 3 && !memcmp (b.data, "key", 3));
  mulib_buffer_free (&b);
  assert (mulib_dom_lookup_namespace_uri (n, "missing", &b, &e) == 0 && !e);
  assert (mulib_dom_set_attribute (n, MULIB_DOM_XMLNS_NAMESPACE, "xmlns:p",
                                   span (NULL, 0), &e));
  assert (mulib_dom_lookup_namespace_uri (n, "p", &b, &e) == 0);
  n = mulib_dom_create_character_data (d, MULIB_DOM_TEXT, NULL,
                                       span (units, 5), &e);
  assert (n && mulib_dom_node_value (n).length == 5);
  assert (mulib_dom_substring_data (n, 1, 2, &b, &e) && b.length == 4);
  assert (!memcmp (b.data, units + 1, 4));
  mulib_buffer_free (&b);
  assert (!mulib_dom_substring_data (n, 6, 0, &b, &e)
          && e == MULIB_DOM_INDEX_SIZE_ERR);
  assert (mulib_dom_replace_data (n, 0, 1, mulib_dom_node_value (n), &e));
  assert (mulib_dom_node_value (n).length == 9);
  assert (mulib_dom_set_node_value (n, span (units, 5), &e));
  tail = mulib_dom_split_text (n, 2, &e);
  assert (tail && mulib_dom_node_value (n).length == 2
          && mulib_dom_node_value (tail).data[0] == 0xde00);
  assert (mulib_dom_string_to_utf8 (mulib_dom_node_value (tail), &b));
  assert (b.length == 5 && !memcmp (b.data, "\xef\xbf\xbd\0B", 5));
  mulib_buffer_free (&b);
  assert (!mulib_dom_create_text_utf8 (d, "\xff", 1, &e)
          && e == MULIB_DOM_INVALID_CHARACTER_ERR);
  {
    const uint16_t cdata_end[] = { ']', ']', '>' }, pi_end[] = { '?', '>' };
    assert (!mulib_dom_create_character_data (d, MULIB_DOM_CDATA, NULL,
                                              span (cdata_end, 3), &e));
    assert (!mulib_dom_create_character_data (
        d, MULIB_DOM_PROCESSING_INSTRUCTION, "target", span (pi_end, 2), &e));
    n = mulib_dom_create_character_data (d, MULIB_DOM_CDATA, NULL,
                                         span (NULL, 0), &e);
    assert (n && mulib_dom_set_node_value (n, span (cdata_end, 3), &e));
    clone = mulib_dom_clone_node (n, 0, &e);
    assert (clone && mulib_dom_is_equal_node (n, clone));
    tail = mulib_dom_split_text (n, 1, &e);
    assert (tail && mulib_dom_node_type (tail) == MULIB_DOM_TEXT);
  }
  assert (mulib_dom_set_text_content (root, span (NULL, 0), &e));
  empty = text (d, "");
  n = text (d, "hello");
  tail = text (d, " world");
  assert (mulib_dom_append_child (root, empty, &e));
  assert (mulib_dom_append_child (root, n, &e));
  assert (mulib_dom_append_child (root, tail, &e));
  assert (mulib_dom_normalize (root, &e));
  assert (mulib_dom_child_count (root) == 1
          && mulib_dom_first_child (root) == n);
  assert (!mulib_dom_parent (empty) && !mulib_dom_parent (tail));
  assert_text (root, "hello world");
  assert (mulib_dom_set_text_content (root, mulib_dom_node_value (n), &e));
  assert_text (root, "hello world");
  assert (!mulib_dom_parent (n));
  assert (mulib_dom_remove_attribute (root, NULL, "id", &e)
          && !mulib_dom_owner_element (a));
  assert (mulib_dom_document_destroy (d)
          && mulib_dom_document_destroy (other));
}

typedef struct event_log
{
  mulib_dom_document_t *document;
  mulib_dom_node_t *root, *child;
  int trace[64], count, once_count;
} event_log;
static void
log_event (mulib_dom_event_t *event, void *userdata)
{
  event_log *l = (event_log *)userdata;
  mulib_buffer_t path = { 0 };
  assert (mulib_dom_event_composed_path (event, &path, NULL));
  assert (path.length == 2 * sizeof (mulib_dom_node_t *));
  assert (((mulib_dom_node_t **)path.data)[0] == l->child
          && ((mulib_dom_node_t **)path.data)[1] == l->root);
  mulib_buffer_free (&path);
  int who = mulib_dom_event_current_target (event) == l->root ? 10 : 20;
  l->trace[l->count++] = who + (int)mulib_dom_event_phase (event);
}
static void
once_event (mulib_dom_event_t *event, void *userdata)
{
  event_log *l = (event_log *)userdata;
  mulib_dom_event_t *nested = mulib_dom_event_new ("once", 0, 0, NULL);
  mulib_dom_error_t e;
  ++l->once_count;
  assert (l->once_count == 1);
  assert (!mulib_dom_event_destroy (event)
          && !mulib_dom_document_destroy (l->document));
  assert (mulib_dom_dispatch_event (l->child, event, &e) == -1
          && e == MULIB_DOM_INVALID_STATE_ERR);
  assert (nested && mulib_dom_dispatch_event (l->child, nested, &e) == 1);
  assert (mulib_dom_event_destroy (nested));
}
static void
cancel_event (mulib_dom_event_t *e, void *p)
{
  (void)p;
  mulib_dom_event_prevent_default (e);
}
static void
stop_event (mulib_dom_event_t *e, void *p)
{
  (void)p;
  mulib_dom_event_stop_propagation (e);
}
static void
mutate_event (mulib_dom_event_t *e, void *p)
{
  event_log *l = (event_log *)p;
  assert (mulib_dom_remove_child (l->root, l->child, NULL));
  mulib_dom_remove_event_listener (l->child, "mutate", log_event, p, 0);
  assert (mulib_dom_add_event_listener (l->child, "mutate", cancel_event, NULL,
                                        NULL, NULL));
  assert (!mulib_dom_event_default_prevented (e));
}
static void
events (void)
{
  event_log log = { 0 };
  mulib_dom_listener_options_t capture = { 1, 0, 0 }, once = { 0, 1, 0 },
                               passive = { 0, 0, 1 };
  mulib_dom_event_t *e;
  log.document = mulib_dom_document_new ();
  log.root = element (log.document, "root");
  log.child = element (log.document, "child");
  assert (mulib_dom_append_child (log.root, log.child, NULL));
  assert (mulib_dom_add_event_listener (log.root, "x", log_event, &log,
                                        &capture, NULL));
  assert (mulib_dom_add_event_listener (log.root, "x", log_event, &log, NULL,
                                        NULL));
  assert (mulib_dom_add_event_listener (log.child, "x", log_event, &log,
                                        &capture, NULL));
  assert (mulib_dom_add_event_listener (log.child, "x", log_event, &log, NULL,
                                        NULL));
  assert (mulib_dom_add_event_listener (log.child, "x", log_event, &log, NULL,
                                        NULL));
  e = mulib_dom_event_new ("x", 1, 1, &log);
  assert (e && mulib_dom_dispatch_event (log.child, e, NULL) == 1);
  assert (log.count == 4 && log.trace[0] == 11 && log.trace[1] == 22
          && log.trace[2] == 22 && log.trace[3] == 13);
  assert (!mulib_dom_event_current_target (e) && !mulib_dom_event_phase (e));
  assert (mulib_dom_event_bubbles (e) && mulib_dom_event_cancelable (e));
  {
    mulib_buffer_t path = { 0 };
    assert (mulib_dom_event_composed_path (e, &path, NULL) && !path.length);
    mulib_buffer_free (&path);
  }
  assert (mulib_dom_event_target (e) == log.child
          && mulib_dom_event_detail (e) == &log);
  assert (mulib_dom_event_destroy (e));
  assert (mulib_dom_add_event_listener (log.child, "once", once_event, &log,
                                        &once, NULL));
  e = mulib_dom_event_new ("once", 0, 0, NULL);
  assert (e && mulib_dom_dispatch_event (log.child, e, NULL) == 1
          && log.once_count == 1);
  assert (mulib_dom_dispatch_event (log.child, e, NULL) == 1
          && log.once_count == 1);
  assert (mulib_dom_event_destroy (e));
  assert (mulib_dom_add_event_listener (log.child, "cancel", cancel_event,
                                        NULL, &passive, NULL));
  e = mulib_dom_event_new ("cancel", 0, 1, NULL);
  assert (e && mulib_dom_dispatch_event (log.child, e, NULL) == 1);
  mulib_dom_remove_event_listener (log.child, "cancel", cancel_event, NULL, 0);
  assert (mulib_dom_add_event_listener (log.child, "cancel", cancel_event,
                                        NULL, NULL, NULL));
  assert (mulib_dom_dispatch_event (log.child, e, NULL) == 0
          && mulib_dom_event_default_prevented (e));
  assert (mulib_dom_event_destroy (e));
  assert (mulib_dom_add_event_listener (log.child, "stop", stop_event, NULL,
                                        &capture, NULL));
  assert (mulib_dom_add_event_listener (log.child, "stop", log_event, &log,
                                        NULL, NULL));
  assert (mulib_dom_add_event_listener (log.root, "stop", log_event, &log,
                                        NULL, NULL));
  log.count = 0;
  e = mulib_dom_event_new ("stop", 1, 0, NULL);
  assert (e && mulib_dom_dispatch_event (log.child, e, NULL) == 1
          && log.count == 1);
  assert (mulib_dom_event_destroy (e));
  assert (mulib_dom_add_event_listener (log.child, "mutate", mutate_event,
                                        &log, NULL, NULL));
  assert (mulib_dom_add_event_listener (log.child, "mutate", log_event, &log,
                                        NULL, NULL));
  assert (mulib_dom_add_event_listener (log.root, "mutate", log_event, &log,
                                        NULL, NULL));
  log.count = 0;
  e = mulib_dom_event_new ("mutate", 1, 1, NULL);
  assert (e && mulib_dom_dispatch_event (log.child, e, NULL) == 1);
  assert (log.count == 1
          && log.trace[0] == 13); /* snapshotted path; new listener deferred */
  assert (mulib_dom_event_destroy (e));
  assert (mulib_dom_document_destroy (log.document));
}
static void
limits_and_failures (void)
{
  mulib_dom_document_t *d = mulib_dom_document_new ();
  mulib_dom_node_t *root = element (d, "root"), *n = root, *child, *copy;
  mulib_dom_error_t e;
  size_t i, total, count;
  for (i = 1; i < MULIB_DOM_DEPTH_LIMIT; ++i)
    {
      child = element (d, "n");
      assert (mulib_dom_append_child (n, child, &e));
      n = child;
    }
  child = element (d, "too-deep");
  assert (!mulib_dom_append_child (n, child, &e) && e == MULIB_DOM_LIMIT_ERR);
  assert (!mulib_dom_parent (child));
  allocations = 0;
  copy = mulib_dom_clone_node (root, 1, &e);
  assert (copy);
  total = allocations;
  count = d->count;
  for (i = 1; i <= total; ++i)
    {
      allocations = 0;
      fail_at = i;
      assert (!mulib_dom_clone_node (root, 1, &e)
              && e == MULIB_DOM_OUT_OF_MEMORY);
      assert (d->count == count && mulib_dom_child_count (root) == 1);
    }
  fail_at = 0;
  assert (mulib_dom_document_destroy (d));
  for (i = 1; i < 16; ++i)
    {
      const uint16_t s[] = { 'x', 'y' };
      d = mulib_dom_document_new ();
      root = element (d, "root");
      assert (mulib_dom_append_child (root, text (d, "first"), NULL));
      assert (mulib_dom_append_child (root, text (d, "second"), NULL));
      count = d->count;
      allocations = 0;
      fail_at = i;
      if (!mulib_dom_set_attribute (root, "urn:test", "p:id", span (s, 2), &e))
        {
          assert (e == MULIB_DOM_OUT_OF_MEMORY
                  && !mulib_dom_attribute_count (root) && count == d->count);
        }
      fail_at = 0;
      allocations = 0;
      fail_at = i;
      if (!mulib_dom_normalize (root, &e))
        assert (e == MULIB_DOM_OUT_OF_MEMORY
                && mulib_dom_child_count (root) == 2);
      fail_at = 0;
      assert_text (root, "firstsecond");
      assert (mulib_dom_document_destroy (d));
    }
  assert (mulib_dom_document_destroy (NULL));
}
int
main (void)
{
  trees ();
  strings_and_attributes ();
  events ();
  limits_and_failures ();
  return 0;
}
