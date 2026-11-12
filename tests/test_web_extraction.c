#include "fixtures/web_primitives_impl.h"
#include <assert.h>

static int
count_html (void *p, const mulib_html_textstream_event_t *e)
{
  assert (e->len > 0);
  (*(size_t *)p)++;
  return 1;
}
static int
count_token (void *p, const mulib_html_token_t *t)
{
  assert (t->length > 0);
  (*(size_t *)p)++;
  return 1;
}
static int
jsonld (void *p, mulib_shylock_span_t span, size_t offset)
{
  assert (offset > 0);
  assert (span.len == 2 && !memcmp (span.data, "{}", 2));
  (*(size_t *)p)++;
  return 1;
}
static void *
make_node (void *p, const mulib_html_tree_node_t *node, void *parent)
{
  (void)parent;
  assert (node);
  (*(size_t *)p)++;
  return p;
}
static int
put_text (void *ctx, void *node, const char *s, size_t n)
{
  (void)ctx;
  assert (node && s && n);
  return 1;
}
static int
llm (void *ctx, const char *request, size_t n, mulib_shylock_buffer_t *out)
{
  (void)ctx;
  assert (request && n);
  return mulib_shylock_text (out, "{}");
}

int
main (void)
{
  mulib_shylock_buffer_t b = { 0 };
  assert (mulib_shylock_text (&b, "abcdefgh"));
  assert (mulib_shylock_reserve (&b, 63));
  for (int i = 0; i < 10; i++)
    {
      size_t len = b.len;
      assert (mulib_shylock_append (&b, b.data, len));
    }
  assert (b.len == 8192 && !memcmp (b.data, b.data + 4096, 4096));
  size_t old = b.len;
  b.limit = old;
  assert (!mulib_shylock_text (&b, "x") && b.len == old);
  assert (!mulib_shylock_json_string (&b, "abc", 3) && b.len == old);
  mulib_shylock_buffer_free (&b);
  b.limit = SIZE_MAX;
  assert (mulib_shylock_json_string (&b, "\"\n", 2)
          && !strcmp (b.data, "\"\\\"\\u000a\""));
  old = b.len;
  assert (!mulib_shylock_json_string (&b, "\xc0\xaf", 2) && b.len == old);
  double number;
  assert (mulib_shylock_parse_decimal (" 1.25e2 ", &number) && number == 125);
  assert (!mulib_shylock_parse_decimal ("0x1p4", &number));
  assert (!mulib_shylock_parse_decimal ("NaN", &number));
  number = 77;
  assert (!mulib_shylock_parse_decimal ("1e-100000", &number) && number == 77);
  assert (mulib_shylock_parse_decimal ("0e-100000", &number) && number == 0);
  mulib_shylock_buffer_free (&b);

  const char *html = "<!doctype html><div ID = 'price' class='a featured "
                     "b'>one<br>two<script>if (a < b) x='<p>';</script></div>";
  size_t count = 0;
  assert (mulib_html_textstream_scan (html, strlen (html), count_html, &count,
                                      100));
  assert (!mulib_html_textstream_scan (html, strlen (html), count_html, &count,
                                       1));
  assert (!mulib_html_textstream_scan ("<!--", 4, count_html, &count, 0));
  assert (!mulib_html_textstream_scan ("<p a='", 6, count_html, &count, 0));
  assert (mulib_html_tokenizer_scan (html, strlen (html), count_token, &count,
                                     100));
  assert (mulib_readability_extract (html, strlen (html), &b)
          && !strcmp (b.data, "one two"));
  mulib_shylock_buffer_free (&b);
  mulib_html_tree_node_t *root = NULL;
  assert (mulib_html_tree_parse (html, strlen (html), 100, 10, &root));
  const mulib_html_tree_node_t *node
      = mulib_dom_query_first (root, ".featured", 100);
  assert (node && node == mulib_dom_query_first (root, "#price", 100));
  assert (!mulib_dom_query_first (root, ".feature", 100));
  assert (mulib_dom_query_all (root, "*", NULL, 0, 100) == 3);
  mulib_html_dombridge_ops_t ops = { make_node, put_text };
  count = 0;
  assert (mulib_html_dombridge_walk (root, &ops, &count, NULL, 20)
          && count > 4);
  assert (!mulib_html_dombridge_walk (root, &ops, &count, NULL, 1));
  mulib_html_tree_free (root);
  root = NULL;
  const char *optional = "<li>a<li>b";
  assert (mulib_html_tree_parse (optional, strlen (optional), 4, 1, &root));
  assert (root->first_child->next_sibling);
  mulib_html_tree_free (root);
  assert (!mulib_html_tree_parse ("<a><b>", 6, 10, 1, &root));
  mulib_webextract_rule_t rule = { ".featured", 1024 };
  assert (mulib_webextract_rules_text (html, strlen (html), &rule, &b)
          && !strcmp (b.data, "onetwo"));
  old = b.len;
  strcpy (rule.selector, "#absent");
  assert (!mulib_webextract_rules_text (html, strlen (html), &rule, &b)
          && b.len == old);
  mulib_shylock_buffer_free (&b);
  const char *ld
      = "<!-- <script type='application/ld+json'>{}</script> --><script "
        "data-type='application/ld+json'>{}</script><script TYPE = "
        "'application/ld+json'>{}</script>";
  count = 0;
  assert (mulib_html_jsonld_scan (ld, strlen (ld), jsonld, &count, 10)
          && count == 1);
  mulib_schemaorg_offer_t offer;
  const char *json
      = "{\"name\":\"A \\u0026 "
        "B\",\"offers\":{\"price\":\"12.30\",\"priceCurrency\":\"USD\"}}";
  assert (mulib_schemaorg_offer_parse (json, strlen (json), &offer)
          && !strcmp (offer.name, "A & B"));
  const char *bad_json = "\"price\":\"1\",\"priceCurrency\":\"USD\"";
  assert (!mulib_schemaorg_offer_parse (bad_json, strlen (bad_json), &offer));
  bad_json = "{\"price\":\"1\",\"priceCurrency\":\"usd\"}";
  assert (!mulib_schemaorg_offer_parse (bad_json, strlen (bad_json), &offer));
  mulib_webextract_evidence_t ev;
  assert (mulib_webextract_evidence_make ("abc", 3, 1, 2, "price", &ev));
  assert (mulib_webextract_evidence_verify ("abc", 3, &ev));
  assert (!mulib_webextract_evidence_verify ("abd", 3, &ev));
  assert (
      !mulib_webextract_evidence_make ("abc", 3, 2, SIZE_MAX, "price", &ev));
  mulib_webextract_candidate_t candidates[]
      = { { 10, .9, 1, 1 }, { 10.1, .8, 0, 1 }, { 99, .1, 0, 0 } };
  size_t selected;
  assert (mulib_webextract_reconcile (candidates, 3, .02, &number, &selected)
          && selected == 0 && number == 10);
  candidates[2].price = NAN;
  assert (
      !mulib_webextract_reconcile (candidates, 3, .02, &number, &selected));
  assert (mulib_webextract_llm_request ("model", "untrusted \"page\"",
                                        "{\"type\":\"object\"}", 100, &b));
  mulib_json_t *request = mulib_json_parse (b.data, b.len, NULL);
  assert (request);
  mulib_json_destroy (request);
  old = b.len;
  assert (!mulib_webextract_llm_request ("model", "page",
                                         "{} , \"escape\":true", 100, &b)
          && b.len == old);
  mulib_shylock_buffer_t response = { 0 };
  assert (mulib_webextract_llm_call (llm, NULL, b.data, b.len, &response)
          && !strcmp (response.data, "{}"));
  mulib_shylock_buffer_free (&b);
  mulib_shylock_buffer_free (&response);
  mulib_crawlarchive_t archive = { 0 };
  archive.max_body = 3;
  assert (mulib_crawlarchive_add (&archive, "https://example.com", 200, "a\0b",
                                  3));
  assert (!mulib_crawlarchive_add (&archive, "https://example.com", 200,
                                   "long", 4));
  const mulib_crawlarchive_record_t *record
      = mulib_crawlarchive_find (&archive, "https://example.com");
  assert (record && record->body.len == 3
          && record->hash == mulib_shylock_hash ("a\0b", 3));
  mulib_crawlarchive_free (&archive);
  return 0;
}
