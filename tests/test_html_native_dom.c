#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t attempts, fail_at;

static void *
test_malloc (size_t n)
{
  return ++attempts == fail_at ? NULL : malloc (n);
}

static void *
test_calloc (size_t n, size_t z)
{
  return ++attempts == fail_at ? NULL : calloc (n, z);
}

static void *
test_realloc (void *old, size_t n)
{
  return ++attempts == fail_at ? NULL : realloc (old, n);
}

#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DOM
#define MULIB_IMPL_HTML_DOMBRIDGE_NATIVE
#include "mulib-html-dombridge.h"
#undef malloc
#undef calloc
#undef realloc

static const mulib_html_dombridge_limits_t normal = { 8192, 128, 32 };

static mulib_dom_node_t *
find (mulib_dom_node_t *root, const char *selector)
{
  mulib_dom_node_t *node = NULL;
  mulib_dom_error_t err;
  assert (mulib_dom_query_native_first (root, selector, 128, &node, &err));
  assert (node && err == MULIB_DOM_OK);
  return node;
}

static void
assert_name (mulib_dom_node_t *node, const char *name)
{
  assert (node && mulib_dom_local_name (node));
  assert (!strcmp (mulib_dom_local_name (node), name));
}

static void
assert_value (const mulib_dom_node_t *node, const char *expected)
{
  mulib_buffer_t buf = { 0 };
  assert (node && mulib_dom_string_to_utf8 (mulib_dom_node_value (node), &buf));
  assert (buf.length == strlen (expected));
  assert (!buf.length || !memcmp (buf.data, expected, buf.length));
  mulib_buffer_free (&buf);
}

static void
test_document (void)
{
  char source[] =
      "\xef\xbb\xbf \r\n<!--before--><!DOCTYPE HTML><HTML lang=en>"
      "<HEAD>\r\n<TITLE>A&amp;B <b>literal</b></TITLE>"
      "<style>p > b { content:'&amp;<div>'; }</STYLE></head>\r\n"
      "<BODY><p ID=first id=ignored CLASS='alpha&#9;second' data-x='&notit;'>"
      "A&#x1f600;<b>B</b><!--not text-->C&NotEqualTilde;"
      "<div id=block><img src='x>y'/><textarea>\r\n&lt;x>&amp;</textarea></div>"
      "<ul><li>one<li>two</ul><dl><dt>a<dd>b</dl>"
      "<pre>\rline 1\r\nline 2</pre>"
      "</body> <!--html comment--></html>\r\n<!--document comment-->";
  mulib_dom_document_t *doc = NULL;
  assert (mulib_html_dombridge_parse (source, strlen (source), &normal, &doc)
          == MULIB_HTML_DOMBRIDGE_OK);
  memset (source, '!', sizeof source);
  mulib_dom_node_t *root = mulib_dom_document_node (doc);
  mulib_dom_node_t *html = mulib_dom_document_element (doc);
  assert_name (html, "html");
  assert (!strcmp (mulib_dom_namespace_uri (html), MULIB_DOM_HTML_NAMESPACE));
  assert (mulib_dom_child_count (root) == 4);
  assert_value (mulib_dom_first_child (root), "before");
  mulib_dom_node_t *doctype = mulib_dom_child_at (root, 1);
  assert (mulib_dom_node_type (doctype) == MULIB_DOM_DOCUMENT_TYPE);
  assert (!strcmp (mulib_dom_node_name (doctype), "html"));
  assert_value (mulib_dom_last_child (root), "document comment");
  assert_value (mulib_dom_last_child (html), "html comment");
  mulib_dom_node_t *head = mulib_dom_first_child (html);
  mulib_dom_node_t *body = find (root, "BODY");
  assert_name (head, "head");
  assert (mulib_dom_parent (body) == html);
  assert_value (mulib_dom_next_sibling (head), "\n");
  assert_value (mulib_dom_first_child (find (head, "title")), "A&B <b>literal</b>");
  assert_value (mulib_dom_first_child (find (head, "style")),
                 "p > b { content:'&amp;<div>'; }");
  assert_value (mulib_dom_first_child (find (body, "textarea")), "<x>&");
  assert_value (mulib_dom_first_child (find (body, "pre")), "line 1\nline 2");
  assert_value (mulib_dom_last_child (body), " \n");
  mulib_dom_node_t *p = find (body, "#first");
  assert (p == find (body, ".second"));
  assert (mulib_dom_owner_document (p) == doc);
  assert (mulib_dom_attribute_count (p) == 3);
  assert_value (mulib_dom_get_attribute_node (p, NULL, "data-x"), "&notit;");
  mulib_dom_node_t *div = find (body, "DIV");
  assert (mulib_dom_next_sibling (p) == div);
  assert_value (mulib_dom_get_attribute_node (find (div, "img"), NULL, "src"), "x>y");
  assert (mulib_dom_child_count (find (body, "ul")) == 2);
  assert (mulib_dom_child_count (find (body, "dl")) == 2);

  mulib_dom_node_t *result = p;
  mulib_dom_error_t err;
  assert (!mulib_dom_query_native_first (body, "p b", 64, &result, &err));
  assert (result == p && err == MULIB_DOM_INVALID_CHARACTER_ERR);
  assert (!mulib_dom_query_native_first (body, "#block", 1, &result, &err));
  assert (result == p && err == MULIB_DOM_LIMIT_ERR);
  assert (mulib_dom_query_native_first (body, "#ignored", 64, &result, &err));
  assert (!result && err == MULIB_DOM_OK);
  assert (mulib_dom_query_native_first (p, "p", 64, &result, &err));
  assert (!result); /* Don't inspect root or its siblings. */
  static const uint16_t new_class[] = { 'l', 'i', 'v', 'e' };
  assert (mulib_dom_set_attribute (p, NULL, "class",
                                  (mulib_dom_string_t){ new_class, 4 }, &err));
  assert (p == find (body, ".live"));

  mulib_dom_node_t *first = mulib_dom_first_child (p), *last = mulib_dom_last_child (p);
  mulib_dom_live_range_t *range = mulib_dom_live_range_new (first, 1, &err);
  assert (range && mulib_dom_live_range_set_end (range, last, 2, &err));
  mulib_buffer_t selected = { 0 };
  static const uint16_t pref = '!';
  assert (mulib_buffer_append (&selected, &pref, sizeof pref));
  assert (!mulib_dom_live_range_text (range, 64, 4, &selected, &err));
  assert (err == MULIB_DOM_LIMIT_ERR && selected.length == sizeof pref);
  assert (!mulib_dom_live_range_text (range, 2, 64, &selected, &err));
  assert (err == MULIB_DOM_LIMIT_ERR && selected.length == sizeof pref);
  assert (mulib_dom_live_range_text (range, 5, 5, &selected, &err));
  static const uint16_t expected[] = { '!', 0xd83d, 0xde00, 'B', 'C', 0x2242 };
  assert (selected.length == sizeof expected
          && !memcmp (selected.data, expected, sizeof expected));
  mulib_buffer_free (&selected);
  mulib_dom_node_t *bold = find (p, "b");
  assert (mulib_dom_remove_child (p, bold, &err));
  assert (mulib_dom_live_range_text (range, 64, 4, &selected, &err));
  static const uint16_t after[] = { 0xd83d, 0xde00, 'C', 0x2242 };
  assert (selected.length == sizeof after && !memcmp (selected.data, after, sizeof after));
  mulib_buffer_free (&selected);
  assert (mulib_dom_live_range_set_start (range, p, 1, &err));
  assert (mulib_dom_live_range_set_end (range, p, 3, &err));
  assert (mulib_dom_live_range_text (range, 2, 3, &selected, &err));
  static const uint16_t final[] = { 'C', 0x2242, 0x0338 };
  assert (selected.length == sizeof final && !memcmp (selected.data, final, sizeof final));
  mulib_buffer_free (&selected);
  assert (mulib_dom_document_destroy (doc));
  assert (!mulib_dom_live_range_text (range, 64, 64, &selected, &err));
  assert (err == MULIB_DOM_INVALID_STATE_ERR && !selected.length);
  mulib_dom_live_range_destroy (range);
}

static void
test_limits (void)
{
  mulib_dom_document_t *doc = NULL;
  const mulib_html_dombridge_limits_t empty = { 0, 4, 3 };
  assert (mulib_html_dombridge_parse (NULL, 0, &empty, &doc)
          == MULIB_HTML_DOMBRIDGE_OK);
  mulib_dom_node_t *html = mulib_dom_document_element (doc);
  assert (mulib_dom_child_count (html) == 2);
  assert_name (mulib_dom_first_child (html), "head");
  assert_name (mulib_dom_last_child (html), "body");
  mulib_dom_document_t *result = doc;
  static const char input[] = "<p id=target>text";
  mulib_html_dombridge_limits_t tight = { sizeof input - 1, 7, 5 };
  assert (mulib_html_dombridge_parse (input, sizeof input - 1, &tight, &result)
          == MULIB_HTML_DOMBRIDGE_OK && result != doc);
  assert (mulib_dom_document_destroy (result));
  result = doc;
  --tight.max_nodes;
  assert (mulib_html_dombridge_parse (input, sizeof input - 1, &tight, &result)
          == MULIB_HTML_DOMBRIDGE_LIMIT && result == doc);
  ++tight.max_nodes;
  --tight.max_depth;
  assert (mulib_html_dombridge_parse (input, sizeof input - 1, &tight, &result)
          == MULIB_HTML_DOMBRIDGE_LIMIT && result == doc);
  ++tight.max_depth;
  --tight.max_bytes;
  assert (mulib_html_dombridge_parse (input, sizeof input - 1, &tight, &result)
          == MULIB_HTML_DOMBRIDGE_LIMIT && result == doc);

  static const struct
  {
    const char *input;
    mulib_html_dombridge_status_t status;
  } failures[] = {
    { "<div id=x><table></table></div>", MULIB_HTML_DOMBRIDGE_UNSUPPORTED },
    { "<script>alert(1)</script>", MULIB_HTML_DOMBRIDGE_UNSUPPORTED },
    { "<a><b><a></a></b></a>", MULIB_HTML_DOMBRIDGE_UNSUPPORTED },
    { "<p/>", MULIB_HTML_DOMBRIDGE_UNSUPPORTED },
    { "<title>missing terminator", MULIB_HTML_DOMBRIDGE_INVALID_INPUT },
    { "<div><b>no adoption agency</div>", MULIB_HTML_DOMBRIDGE_INVALID_INPUT },
    { "<p title='missing>", MULIB_HTML_DOMBRIDGE_INVALID_INPUT },
    { "<!doctype html PUBLIC 'x'>", MULIB_HTML_DOMBRIDGE_UNSUPPORTED },
    { "<p>\xc0\x80", MULIB_HTML_DOMBRIDGE_INVALID_INPUT },
    { "<!--->should not be consumed-->", MULIB_HTML_DOMBRIDGE_INVALID_INPUT },
  };
  for (size_t i = 0; i < sizeof failures / sizeof failures[0]; ++i)
    {
      assert (mulib_html_dombridge_parse (failures[i].input,
                                         strlen (failures[i].input),
                                         &normal, &result) == failures[i].status);
      assert (result == doc);
    }
  assert (mulib_html_dombridge_parse ("<p>\0", 4, &normal, &result)
          == MULIB_HTML_DOMBRIDGE_INVALID_INPUT && result == doc);
  assert (mulib_html_dombridge_parse ("", 0, NULL, &result)
          == MULIB_HTML_DOMBRIDGE_INVALID_ARGUMENT && result == doc);
  assert (mulib_dom_document_destroy (doc));
}

static void
test_allocations (void)
{
  static const char html[] =
      "<!doctype html><!--demo--><title>&amp;</title><p id='x&#121;'>\r\n"
      "text</body> <!--trailing--></html>\n";
  mulib_dom_document_t *sentinel = mulib_dom_document_new ();
  assert (sentinel);
  mulib_dom_document_t *out = sentinel;
  attempts = 0;
  assert (mulib_html_dombridge_parse (html, sizeof html - 1, &normal, &out)
          == MULIB_HTML_DOMBRIDGE_OK);
  size_t count = attempts;
  mulib_dom_node_t *p = find (mulib_dom_document_node (out), "#xy");
  assert (mulib_dom_child_count (p) == 1);
  assert_value (mulib_dom_first_child (p), "\ntext \n");
  assert (mulib_dom_document_destroy (out));

  for (fail_at = 1; fail_at <= count; ++fail_at)
    {
      attempts = 0;
      out = sentinel;
      assert (mulib_html_dombridge_parse (html, sizeof html - 1, &normal, &out)
              == MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY);
      assert (out == sentinel);
    }
  fail_at = 0;
  assert (mulib_dom_document_destroy (sentinel));
}

int
main (void)
{
  test_document ();
  test_limits ();
  test_allocations ();
  puts ("native HTML document, query and range checks passed");
  return 0;
}
