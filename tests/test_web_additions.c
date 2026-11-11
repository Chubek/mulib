/* Cross-module regressions, boundary checks and deterministic allocation
 * faults. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t fail_after = SIZE_MAX, live_allocations;
static int
allocation_allowed (void)
{
  if (fail_after == SIZE_MAX)
    return 1;
  if (!fail_after)
    return 0;
  --fail_after;
  return 1;
}
static void *
web_malloc (size_t size)
{
  if (!allocation_allowed ())
    return NULL;
  void *p = malloc (size);
  if (p)
    ++live_allocations;
  return p;
}
static void *
web_realloc (void *p, size_t size)
{
  if (!allocation_allowed ())
    return NULL;
  int fresh = p == NULL;
  void *q = realloc (p, size);
  if (q && fresh)
    ++live_allocations;
  return q;
}
static void
web_free (void *p)
{
  if (p)
    {
      assert (live_allocations);
      --live_allocations;
    }
  free (p);
}
#define malloc web_malloc
#define realloc web_realloc
#define free web_free
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_URLCANON
#define MULIB_IMPL_MIME
#define MULIB_IMPL_CHARSET
#define MULIB_IMPL_REDIRECT
#define MULIB_IMPL_COOKIE
#define MULIB_IMPL_COOKIEJAR
#define MULIB_IMPL_SEARCH_QUERY
#define MULIB_IMPL_SITEMAP
#define MULIB_IMPL_ROBOTS
#define MULIB_IMPL_NETPOLICY
#define MULIB_IMPL_WEBRATELIMIT
#define MULIB_IMPL_CRAWLQUEUE
#define MULIB_IMPL_SCRAPE_METADATA
#define MULIB_IMPL_SCRAPE_READABLE
#define MULIB_IMPL_MULTIPART
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BROWSERLOCATOR
#define MULIB_IMPL_BROWSERWAIT
#define MULIB_IMPL_WEBSTORAGE
#define MULIB_IMPL_WEBTIMERS
#define MULIB_IMPL_BROWSERSNAPSHOT
#define MULIB_IMPL_WEBSEARCH
#define MULIB_IMPL_SCRAPE_RULE
#define MULIB_IMPL_SCRAPE_TABLE
#define MULIB_IMPL_WEBEXTRACT
#define MULIB_IMPL_SEARCH_RANK
#define MULIB_IMPL_DOMSERIALIZE
#define MULIB_IMPL_WEBLOCATION
#define MULIB_IMPL_BROWSERCONTEXT
#define MULIB_IMPL_BROWSERTRACE
#define MULIB_IMPL_SEARCH_SERP
#define MULIB_IMPL_SCRAPE_SCHEMA
#define MULIB_IMPL_BROWSERSCREENSHOT
#define MULIB_IMPL_SEARCH_SESSION
#define MULIB_IMPL_SCRAPE
#define MULIB_IMPL_WEBARCHIVE
#include "mulib-browsercontext.h"
#include "mulib-browserlocator.h"
#include "mulib-browserscreenshot.h"
#include "mulib-browsersnapshot.h"
#include "mulib-browsertrace.h"
#include "mulib-browserwait.h"
#include "mulib-charset.h"
#include "mulib-cookie.h"
#include "mulib-cookiejar.h"
#include "mulib-crawlqueue.h"
#include "mulib-domserialize.h"
#include "mulib-mime.h"
#include "mulib-multipart.h"
#include "mulib-netpolicy.h"
#include "mulib-redirect.h"
#include "mulib-robots.h"
#include "mulib-scrape-metadata.h"
#include "mulib-scrape-readable.h"
#include "mulib-scrape-rule.h"
#include "mulib-scrape-schema.h"
#include "mulib-scrape-table.h"
#include "mulib-scrape.h"
#include "mulib-search-query.h"
#include "mulib-search-rank.h"
#include "mulib-search-serp.h"
#include "mulib-search-session.h"
#include "mulib-sitemap.h"
#include "mulib-urlcanon.h"
#include "mulib-webarchive.h"
#include "mulib-webextract.h"
#include "mulib-weblocation.h"
#include "mulib-webratelimit.h"
#include "mulib-websearch.h"
#include "mulib-webstorage.h"
#include "mulib-webtimers.h"
#undef malloc
#undef realloc
#undef free

static void
urls (void)
{
  const char *invalid[] = { "",
                            "http://",
                            "ftp://example.com",
                            "http://:80/",
                            "http://user@example.com/",
                            "http://example.com:/",
                            "http://example.com:0/",
                            "http://example.com:65536/",
                            "http://example.com:12345678901234567890/",
                            "http://ex ample.com/",
                            "http://example.com\\@localhost/",
                            "http://example.com/%",
                            "http://example.com/%0z",
                            "http://-example.com/",
                            "http://example..com/",
                            "http://[::1/",
                            "http://[1:2]/",
                            "http://[1::2::3]/",
                            "http://[::]:/" };
  char b[256];
  for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
    {
      strcpy (b, "unchanged");
      assert (!mulib_urlcanon_normalize (invalid[i], b, sizeof b));
      assert (!strcmp (b, "unchanged"));
    }
  assert (!mulib_urlcanon_normalize (NULL, b, sizeof b));
  assert (!mulib_urlcanon_normalize ("http://x", NULL, sizeof b));
  assert (mulib_urlcanon_normalize ("HTTP://Example.COM:00080?x=%2F#f", b,
                                    sizeof b));
  assert (!strcmp (b, "http://example.com/?x=%2F"));
  assert (mulib_urlcanon_normalize ("https://EXAMPLE.com#f", b, sizeof b));
  assert (!strcmp (b, "https://example.com/"));
  assert (mulib_urlcanon_normalize ("http://[2001:DB8::1]:80/a", b, sizeof b));
  assert (!strcmp (b, "http://[2001:db8::1]/a"));
  assert (mulib_urlcanon_normalize ("http://[::1]:8080", b, sizeof b));
  assert (!strcmp (b, "http://[::1]:8080/"));
  strcpy (b, "HTTP://x?y#f");
  assert (mulib_urlcanon_normalize (b, b, sizeof b));
  assert (!strcmp (b, "http://x/?y"));
  strcpy (b, "sentinel");
  assert (!mulib_urlcanon_normalize ("http://x", b, 9));
  assert (!strcmp (b, "sentinel"));
  assert (mulib_urlcanon_normalize ("http://x", b, 10));
  char longurl[8194];
  memset (longurl, 'a', sizeof longurl);
  memcpy (longurl, "http://x/", 9);
  longurl[8193] = 0;
  assert (!mulib_urlcanon_normalize (longurl, b, sizeof b));
  const char *blocked[] = { "http://127.0.0.1/",
                            "http://10.1.2.3",
                            "http://172.16.0.1",
                            "http://192.168.1.1",
                            "http://169.254.169.254",
                            "http://100.64.1.1",
                            "http://0.1.2.3",
                            "http://224.0.0.1",
                            "http://198.18.0.1",
                            "http://192.0.2.1",
                            "http://localhost.",
                            "http://sub.localhost",
                            "http://foo.local.",
                            "http://example.com@127.0.0.1",
                            "http://[::1]",
                            "http://[::ffff:127.0.0.1]",
                            "http://[2606:4700::1111]",
                            "http://2130706433",
                            "http://127.1",
                            "http://0177.0.0.1",
                            "http://0x7f.0.0.1",
                            "http://0x7f000001",
                            "http://999.1.1.1",
                            "http://1.2.3.4.5",
                            "http://intranet" };
  for (size_t i = 0; i < sizeof blocked / sizeof blocked[0]; ++i)
    assert (!mulib_netpolicy_public_url (blocked[i]));
  assert (!mulib_netpolicy_public_url (NULL));
  assert (mulib_netpolicy_public_url ("HTTPS://Example.COM.:443/path"));
  assert (mulib_netpolicy_public_url ("https://8.8.8.8/"));
  assert (mulib_netpolicy_public_url ("http://172.32.0.1:8080/"));
}

static void
headers (void)
{
  char b[128], name[32], value[32];
  assert (!strcmp (mulib_mime_type ("dir.txt/PIC.PNG"), "image/png"));
  assert (
      !strcmp (mulib_mime_type ("dir.png/file"), "application/octet-stream"));
  assert (!strcmp (mulib_mime_type (NULL), "application/octet-stream"));
  assert (mulib_mime_content_type (" \tApplication/Vnd.foo+JSON \t; charset=x",
                                   b, sizeof b));
  assert (!strcmp (b, "application/vnd.foo+json"));
  assert (mulib_mime_content_type ("a/b", b, 4));
  const char *badtypes[]
      = { "/abc", "abc/", "abc", "a/b/c", "a/b junk", "a /b", "a/b\r\nX:y" };
  for (size_t i = 0; i < sizeof badtypes / sizeof badtypes[0]; ++i)
    {
      strcpy (b, "keep");
      assert (!mulib_mime_content_type (badtypes[i], b, sizeof b));
      assert (!strcmp (b, "keep"));
    }
  strcpy (b, "keep");
  assert (!mulib_mime_content_type ("text/html", b, 9));
  assert (!strcmp (b, "keep"));
  assert (!mulib_mime_content_type (NULL, b, sizeof b));
  assert (mulib_charset_extract (
      "text/html; title=\"x; charset=bogus\"; CHARSET = \"UTF-8\"", b,
      sizeof b));
  assert (!strcmp (b, "utf-8"));
  assert (mulib_charset_extract ("text/html; charset=ISO-8859-1; x=\"a\\\"b\"",
                                 b, sizeof b));
  assert (!strcmp (b, "iso-8859-1"));
  const char *badcharsets[] = { "",
                                "text/html",
                                "text/html; charset",
                                "text/html; charset ",
                                "text/html; charset=",
                                "text/html; charset=\"UTF-8",
                                "text/html; charset=\"\"",
                                "text/html; charset=utf-8 junk",
                                "text/html; charset=\"utf 8\"",
                                "text/html; x=\"a; charset=utf-8\"",
                                "text/html; charset=utf-8; charset=latin1",
                                "text/html; charset=utf-8\n" };
  for (size_t i = 0; i < sizeof badcharsets / sizeof badcharsets[0]; ++i)
    {
      strcpy (b, "keep");
      assert (!mulib_charset_extract (badcharsets[i], b, sizeof b));
      assert (!strcmp (b, "keep"));
    }
  assert (!mulib_charset_extract ("x; charset=utf-8", b, 5));
  assert (mulib_charset_extract ("x; charset=utf-8", b, 6));
  assert (!mulib_charset_extract (NULL, b, sizeof b));
  assert (mulib_cookie_pair ("a=\"b=c\"; Path=/", name, sizeof name, value,
                             sizeof value));
  assert (!strcmp (name, "a") && !strcmp (value, "\"b=c\""));
  assert (mulib_cookie_pair ("a=", name, 2, value, 1) && !value[0]);
  const char *badcookies[] = { "",       "=b",    "a",
                               "a b=c",  "a=b c", "a=\"b",
                               "a=b\\c", "a=b,c", "a=b; X=\r\nInjected" };
  for (size_t i = 0; i < sizeof badcookies / sizeof badcookies[0]; ++i)
    {
      strcpy (name, "keep");
      strcpy (value, "keep");
      assert (!mulib_cookie_pair (badcookies[i], name, sizeof name, value,
                                  sizeof value));
      assert (!strcmp (name, "keep") && !strcmp (value, "keep"));
    }
  assert (!mulib_cookie_pair ("a=b", name, 1, value, sizeof value));
  assert (!mulib_cookie_pair ("a=b", name, sizeof name, value, 1));
  assert (!mulib_cookie_pair (NULL, name, sizeof name, value, sizeof value));
  for (int status = 100; status <= 599; ++status)
    {
      assert (mulib_redirect_status (status)
              == (status == 301 || status == 302 || status == 303
                  || status == 307 || status == 308));
      assert (mulib_redirect_post_to_get (status)
              == (status == 301 || status == 302 || status == 303));
    }
}

static void
extraction (void)
{
  char b[256];
  const char *meta
      = "<!-- <meta name='description' content='wrong'> -->"
        "<script>var x=\"<meta name='description' content='wrong'>\";</script>"
        "<metadata name='description' content='wrong'>"
        "<meta data-name='description' content='wrong'>"
        "<META CONTENT = 'a > b &amp; c' NAME = \"Description\">";
  assert (mulib_scrape_metadata_get (meta, "description", b, sizeof b));
  assert (!strcmp (b, "a > b &amp; c"));
  strcpy (b, "keep");
  assert (!mulib_scrape_metadata_get (meta, "description", b, 2));
  assert (!strcmp (b, "keep"));
  assert (!mulib_scrape_metadata_get ("<meta name='x'><div content='y'>", "x",
                                      b, sizeof b));
  assert (!mulib_scrape_metadata_get ("<meta name='x' content='unterminated>",
                                      "x", b, sizeof b));
  assert (!mulib_scrape_metadata_get (NULL, "x", b, sizeof b));
  const char *links
      = "<!-- <a href='wrong'> --><style><a href='wrong'></style>"
        "<script><a href='wrong'></script><abbr href='wrong'><a "
        "data-href='wrong'>"
        "<A title='a > b' HREF = 'one'><a href=\"two&amp;x\"><a href=''>";
  assert (mulib_search_serp_href (links, 0, b, sizeof b)
          && !strcmp (b, "one"));
  assert (mulib_search_serp_href (links, 1, b, sizeof b)
          && !strcmp (b, "two&amp;x"));
  assert (mulib_search_serp_href (links, 2, b, 1) && !b[0]);
  assert (!mulib_search_serp_href (links, 3, b, sizeof b));
  assert (!mulib_search_serp_href (links, SIZE_MAX, b, sizeof b));
  assert (!mulib_search_serp_href ("<a href='one'>", 0, b, 3));
  assert (!mulib_search_serp_href (NULL, 0, b, sizeof b));
  mulib_webextract_span_t span = { NULL, 0 };
  const char *jsonld
      = "<!-- <script type='application/ld+json'>wrong</script> -->"
        "<script data-type='application/ld+json'>wrong</script>"
        "<script>var x=\"application/ld+json\";</script>"
        "<SCRIPT TYPE = 'APPLICATION/LD+JSON' data-x='>'>{\"x\":1}</SCRIPT >";
  assert (mulib_webextract_jsonld (jsonld, &span));
  assert (span.length == 7 && !memcmp (span.data, "{\"x\":1}", 7));
  const char *saved = span.data;
  assert (!mulib_webextract_jsonld (
      "<script type='application/ld+json'>broken", &span));
  assert (span.data == saved && span.length == 7);
  assert (!mulib_webextract_jsonld (
      "<script title='application/ld+json'>{}</script>", &span));
  assert (!mulib_webextract_jsonld (NULL, &span));
  assert (mulib_scrape_table_cells (
              "<!-- <td> --><script><th></script><style><td></style>"
              "<div title='<td>'></div><TD>x</TD><th/>y<tdx><td")
          == 2);
  assert (!mulib_scrape_table_cells (NULL));
  const char *html
      = " <p>a</p><p>b</p> <SCRIPT data-x='>'>hidden <b>x</b></SCRIPT>"
        "<STYLE>hidden</STYLE><!-- hidden --> <b>c &amp; d</b> ";
  assert (mulib_scrape_readable (html, b, sizeof b) == 13);
  assert (!strcmp (b, "a b c &amp; d"));
  char copy[256];
  strcpy (copy, html);
  assert (mulib_scrape_readable (copy, copy, sizeof copy) == strlen (b)
          && !strcmp (copy, b));
  assert (mulib_scrape_text (html, copy, sizeof copy) == strlen (b)
          && !strcmp (copy, b));
  assert (mulib_scrape_readable ("a <script>unclosed", b, sizeof b) == 1
          && !strcmp (b, "a"));
  assert (mulib_scrape_readable ("a > b", b, sizeof b) == 5
          && !strcmp (b, "a > b"));
  assert (mulib_scrape_readable ("abc", b, 1) == 0 && !b[0]);
  assert (mulib_scrape_readable ("abc", b, 3) == 2 && !strcmp (b, "ab"));
  assert (!mulib_scrape_readable (NULL, b, sizeof b));
  assert (!mulib_scrape_text ("x", b, 0));
  const char xml[] = "<loc>one</loc><loc>two&amp;x</loc>";
  size_t pos = 0;
  assert (mulib_sitemap_next (xml, sizeof xml - 1, &pos, b, sizeof b)
          && !strcmp (b, "one"));
  size_t savedpos = pos;
  strcpy (b, "keep");
  assert (!mulib_sitemap_next (xml, sizeof xml - 1, &pos, b, 2));
  assert (pos == savedpos && !strcmp (b, "keep"));
  assert (mulib_sitemap_next (xml, sizeof xml - 1, &pos, b, sizeof b)
          && !strcmp (b, "two&amp;x"));
  assert (!mulib_sitemap_next (xml, sizeof xml - 1, &pos, b, sizeof b));
  for (size_t n = 0; n < sizeof xml - 1; ++n)
    {
      pos = n;
      assert (!mulib_sitemap_next (xml, n, &pos, b, sizeof b) && pos == n);
    }
  const char raw[]
      = { '<', 'l', 'o', 'c', '>', 'x', '<', '/', 'l', 'o', 'c', '>' };
  pos = 0;
  assert (mulib_sitemap_next (raw, sizeof raw, &pos, b, sizeof b)
          && !strcmp (b, "x"));
  pos = 0;
  assert (!mulib_sitemap_next ("<loc></loc>", 11, &pos, b, sizeof b));
  pos = 0;
  assert (!mulib_sitemap_next ("<loc><x></loc>", 13, &pos, b, sizeof b));
}

static void
rules_and_formatting (void)
{
  assert (mulib_robots_allowed ("", "Bot", "/"));
  assert (!mulib_robots_allowed (NULL, "Bot", "/"));
  assert (!mulib_robots_allowed (
      "User-agent: Bot\nUser-agent: Other\nDisallow: /secret # comment\r\n",
      "bOt", "/secret/x"));
  assert (mulib_robots_allowed (
      "User-agent: *\nDisallow: /\nUser-agent: Bot\nDisallow:\n", "Bot",
      "/x"));
  assert (!mulib_robots_allowed (
      "User-agent: *\nDisallow: /\nUser-agent: Bot\nDisallow:\n", "Other",
      "/x"));
  assert (mulib_robots_allowed (
      "User-agent: *\nDisallow: /x\nAllow: /x\nDisallow: /x\n", "Bot",
      "/xyz"));
  assert (mulib_robots_allowed ("User-agent: *\nDisallow: /\nAllow: /public\n",
                                "Bot", "/public/a"));
  assert (!mulib_robots_allowed (
      "User-agent: Bot\nAllow: /\nUser-agent: Bot\nDisallow: /private\n",
      "Bot", "/private/a"));
  assert (!mulib_robots_allowed ("User-agent: *\nDisallow: /Long\n", "Bot",
                                 "/Long"));
  char b[512];
  assert (mulib_search_query_url ("google", "a b+&\xc3\xa9", b, sizeof b));
  assert (!strcmp (b, "https://www.google.com/search?q=a+b%2B%26%C3%A9"));
  size_t n = strlen (b);
  strcpy (b, "keep");
  assert (!mulib_search_query_url ("google", "a b+&\xc3\xa9", b, n));
  assert (!strcmp (b, "keep"));
  assert (mulib_search_query_url ("bing", "", b, sizeof b));
  assert (!strcmp (b, "https://www.bing.com/search?q="));
  assert (!mulib_search_query_url ("unknown", "x", b, sizeof b));
  assert (!mulib_search_query_url (NULL, "x", b, sizeof b));
  assert (mulib_search_rank_count ("aaa", "aa") == 2);
  assert (mulib_search_rank_count ("heLLo HELLO", "hello") == 2);
  assert (!mulib_search_rank_count ("a", "abc"));
  assert (!mulib_search_rank_count ("a", ""));
  assert (!mulib_search_rank_count (NULL, "a"));
  assert (mulib_domserialize_escape ("<&>\"'", b, sizeof b) == 24);
  assert (!strcmp (b, "&lt;&amp;&gt;&quot;&#39;"));
  n = strlen (b);
  strcpy (b, "keep");
  assert (!mulib_domserialize_escape ("<&>\"'", b, n) && !strcmp (b, "keep"));
  assert (mulib_domserialize_escape ("<&>\"'", b, n + 1) == n);
  assert (!mulib_domserialize_escape ("", b, 1) && !b[0]);
  assert (!mulib_domserialize_escape (NULL, b, sizeof b));
  n = mulib_multipart_field ("abc", "title", "xyz", b, sizeof b);
  assert (n
          && !strcmp (b, "--abc\r\nContent-Disposition: form-data; "
                         "name=\"title\"\r\n\r\nxyz\r\n"));
  strcpy (b, "keep");
  assert (!mulib_multipart_field ("abc", "title", "xyz", b, n)
          && !strcmp (b, "keep"));
  assert (!mulib_multipart_field ("", "title", "xyz", b, sizeof b));
  assert (!mulib_multipart_field ("abc", "x\\", "xyz", b, sizeof b));
  assert (!mulib_multipart_field ("abc", "x\r\nInjected", "xyz", b, sizeof b));
  assert (!mulib_multipart_field ("abc", "x", "hi\r\n--abc\r\nInjected", b,
                                  sizeof b));
  n = mulib_webarchive_record_header ("https://a/", SIZE_MAX, b, sizeof b);
  assert (n && strstr (b, "WARC-Target-URI: https://a/\r\n")
          && strstr (b, "Content-Length:"));
  strcpy (b, "keep");
  assert (!mulib_webarchive_record_header ("https://a/", SIZE_MAX, b, n)
          && !strcmp (b, "keep"));
  assert (!mulib_webarchive_record_header ("https://a/\r\nInjected", 1, b,
                                           sizeof b));
  assert (!mulib_webarchive_record_header ("", 0, b, sizeof b));
}

static void
state_and_clocks (void)
{
  mulib_webratelimit_t rate;
  mulib_webratelimit_init (&rate, 10);
  assert (mulib_webratelimit_take (&rate, 0));
  assert (!mulib_webratelimit_take (&rate, 9));
  assert (mulib_webratelimit_take (&rate, 10));
  assert (!mulib_webratelimit_take (&rate, UINT64_MAX) && rate.next_ms == 20);
  mulib_webratelimit_init (&rate, 0);
  assert (mulib_webratelimit_take (&rate, UINT64_MAX));
  assert (!mulib_webratelimit_take (NULL, 0));
  mulib_browserwait_t wait = { 0 };
  assert (mulib_browserwait_poll (&wait, 0) == -1);
  assert (mulib_browserwait_init (&wait, 10, 20, 5));
  assert (mulib_browserwait_poll (&wait, 10) == 1
          && mulib_browserwait_poll (&wait, 10) == 0);
  assert (mulib_browserwait_poll (&wait, 30) == 1
          && mulib_browserwait_poll (&wait, 31) == -1);
  assert (!mulib_browserwait_init (&wait, UINT64_MAX, 1, 1));
  assert (!mulib_browserwait_init (&wait, 0, 10, 0));
  assert (mulib_browserwait_init (&wait, UINT64_MAX - 2, 2, 10));
  assert (mulib_browserwait_poll (&wait, UINT64_MAX - 2) == 1
          && wait.next_poll_ms == UINT64_MAX);
  assert (mulib_browserwait_init (&wait, 0, 0, 1)
          && mulib_browserwait_poll (&wait, 0) == 1);
  mulib_webtimers_t timers = { 0 };
  uint64_t ids[64];
  for (size_t i = 0; i < 64; ++i)
    {
      ids[i] = mulib_webtimers_add (&timers, 100, 20);
      assert (ids[i]);
    }
  assert (!mulib_webtimers_add (&timers, 0, 0));
  assert (mulib_webtimers_cancel (&timers, ids[1]));
  assert (!mulib_webtimers_cancel (&timers, ids[1]));
  assert (!mulib_webtimers_next (&timers, 119));
  for (size_t i = 0; i < 64; ++i)
    if (i != 1)
      assert (mulib_webtimers_next (&timers, 120) == ids[i]);
  assert (!mulib_webtimers_next (&timers, 120));
  assert (!mulib_webtimers_add (&timers, UINT64_MAX, 1));
  timers.next_id = UINT64_MAX;
  assert (!mulib_webtimers_add (&timers, 0, 0));
  mulib_browsertrace_t trace = { 0 };
  assert (!mulib_browsertrace_get (&trace, 0));
  for (uint32_t i = 0; i < 300; ++i)
    mulib_browsertrace_push (&trace, (mulib_browsertrace_event_t){ i, 1, i });
  assert (trace.count == 128);
  for (size_t i = 0; i < 128; ++i)
    assert (mulib_browsertrace_get (&trace, i)->time_ms == 172 + i);
  assert (!mulib_browsertrace_get (&trace, 128)
          && !mulib_browsertrace_get (&trace, SIZE_MAX));
  mulib_browsercontext_t context = { 0 };
  assert (!mulib_browsercontext_init (&context, 0, 1) && !context.id);
  assert (mulib_browsercontext_init (&context, UINT64_MAX, -2)
          && context.private_mode == 1);
  assert (!mulib_browsercontext_init (NULL, 1, 0));
  assert (mulib_websearch_known (MULIB_WEBSEARCH_GOOGLE));
  assert (!strcmp (mulib_websearch_provider_name (MULIB_WEBSEARCH_GOOGLE),
                   "google"));
  assert (!mulib_websearch_known ((mulib_websearch_provider_t)0));
  assert (!mulib_websearch_provider_name ((mulib_websearch_provider_t)999));
  size_t bytes = 42;
  assert (!mulib_browserscreenshot_size (0, 1, 4, &bytes) && bytes == 42);
  assert (!mulib_browserscreenshot_size (1, 1, 2, &bytes));
  assert (!mulib_browserscreenshot_size (SIZE_MAX, SIZE_MAX, 4, &bytes));
  assert (!mulib_browserscreenshot_size (1, 1, 4, NULL));
  assert (mulib_browserscreenshot_size (32768, 32768, 3, &bytes)
          && bytes == (size_t)32768 * 32768 * 3);
  mulib_search_session_t session = { 0 };
  assert (!mulib_search_session_init (&session, 0, 10));
  assert (!mulib_search_session_init (&session, 1, 0));
  assert (mulib_search_session_init (&session, 10, 25));
  assert (mulib_search_session_next (&session) == 10
          && mulib_search_session_next (&session) == 10);
  assert (mulib_search_session_next (&session) == 5
          && !mulib_search_session_next (&session));
  assert (mulib_search_session_init (&session, SIZE_MAX, SIZE_MAX));
  assert (mulib_search_session_next (&session) == SIZE_MAX
          && !mulib_search_session_next (&session));
  assert (mulib_scrape_schema_string (NULL, 0, 0));
  assert (!mulib_scrape_schema_string (NULL, 1, 10));
  assert (mulib_scrape_schema_string ("", 0, 0));
  assert (!mulib_scrape_schema_string ("", 1, 10));
  assert (mulib_scrape_schema_string ("abc", 1, 3));
  assert (!mulib_scrape_schema_string ("abc", 1, 2));
}

static void
ownership (void)
{
  mulib_cookiejar_t jar = { 0 };
  mulib_webstorage_t storage = { 0 };
  mulib_crawlqueue_t queue = { 0 };
  char key[32], value[32];
  for (size_t i = 0; i < 40; ++i)
    {
      snprintf (key, sizeof key, "key%zu", i);
      snprintf (value, sizeof value, "value%zu", i);
      assert (mulib_cookiejar_set (&jar, key, value));
      assert (mulib_webstorage_set (&storage, key, value));
      assert (mulib_crawlqueue_push (&queue, key) == 1);
    }
  for (size_t i = 0; i < 40; ++i)
    {
      snprintf (key, sizeof key, "key%zu", i);
      snprintf (value, sizeof value, "value%zu", i);
      assert (!strcmp (mulib_cookiejar_get (&jar, key), value));
      assert (!strcmp (mulib_webstorage_get (&storage, key), value));
      assert (!strcmp (mulib_crawlqueue_next (&queue), key));
      assert (mulib_crawlqueue_push (&queue, key) == 2);
    }
  assert (!mulib_crawlqueue_next (&queue));
  assert (
      mulib_cookiejar_set (&jar, "key0", mulib_cookiejar_get (&jar, "key0")));
  assert (mulib_webstorage_set (&storage, "key0",
                                mulib_webstorage_get (&storage, "key0")));
  assert (mulib_webstorage_set (&storage, "", ""));
  assert (!mulib_cookiejar_set (&jar, "", ""));
  assert (!mulib_cookiejar_get (&jar, "absent")
          && !mulib_webstorage_get (&storage, "absent"));
  assert (!mulib_crawlqueue_push (&queue, ""));
  mulib_cookiejar_free (&jar);
  mulib_cookiejar_free (&jar);
  mulib_webstorage_free (&storage);
  mulib_webstorage_free (&storage);
  mulib_crawlqueue_free (&queue);
  mulib_crawlqueue_free (&queue);
  assert (!jar.length && !storage.len && !queue.len);
  mulib_browserlocator_t locator = { 0 };
  assert (mulib_browserlocator_init (&locator, MULIB_BROWSERLOCATOR_CSS,
                                     "article a"));
  assert (mulib_browserlocator_init (&locator, MULIB_BROWSERLOCATOR_TEXT,
                                     locator.selector + 8));
  assert (!strcmp (locator.selector, "a"));
  assert (!mulib_browserlocator_init (&locator, (mulib_browserlocator_kind_t)0,
                                      "x"));
  assert (!strcmp (locator.selector, "a"));
  mulib_browserlocator_free (&locator);
  mulib_browserlocator_free (&locator);
  mulib_scrape_rule_t rule = { 0 };
  assert (mulib_scrape_rule_init (&rule, "a", "href", 12));
  assert (mulib_scrape_rule_init (&rule, rule.selector, rule.attribute, 0)
          && !rule.all);
  assert (!strcmp (rule.selector, "a") && !strcmp (rule.attribute, "href"));
  assert (mulib_scrape_rule_init (&rule, "p", NULL, 1) && !rule.attribute);
  mulib_scrape_rule_free (&rule);
  mulib_scrape_rule_free (&rule);
  mulib_browsersnapshot_t snapshot = { 0 };
  assert (mulib_browsersnapshot_create (&snapshot, "abc", 3));
  assert (snapshot.hash == UINT64_C (0xe71fa2190541574b));
  assert (mulib_browsersnapshot_create (&snapshot, snapshot.data + 1, 2));
  assert (snapshot.size == 2 && !memcmp (snapshot.data, "bc", 2));
  assert (!mulib_browsersnapshot_create (&snapshot, NULL, 1)
          && snapshot.size == 2);
  assert (!mulib_browsersnapshot_create (&snapshot, "x", 67108865));
  assert (mulib_browsersnapshot_create (&snapshot, NULL, 0));
  assert (snapshot.hash == UINT64_C (14695981039346656037));
  mulib_browsersnapshot_free (&snapshot);
  mulib_browsersnapshot_free (&snapshot);
  mulib_weblocation_t location = { 0 };
  assert (mulib_weblocation_set (&location, "HTTP://Example.com"));
  assert (mulib_weblocation_set (&location, location.href));
  assert (!strcmp (location.href, "HTTP://Example.com"));
  assert (!mulib_weblocation_set (&location, "://broken")
          && !strcmp (location.href, "HTTP://Example.com"));
  mulib_weblocation_free (&location);
  mulib_weblocation_free (&location);
  assert (!live_allocations);
}

static void
allocation_failures (void)
{
  for (size_t fault = 0; fault < 5; ++fault)
    {
      mulib_cookiejar_t jar = { 0 };
      mulib_webstorage_t storage = { 0 };
      mulib_crawlqueue_t queue = { 0 };
      fail_after = fault;
      int ok = mulib_cookiejar_set (&jar, "name", "value");
      assert (ok ? jar.length == 1 : jar.length == 0);
      fail_after = SIZE_MAX;
      mulib_cookiejar_free (&jar);
      assert (!live_allocations);
      fail_after = fault;
      ok = mulib_webstorage_set (&storage, "name", "value");
      assert (ok ? storage.len == 1 : storage.len == 0);
      fail_after = SIZE_MAX;
      mulib_webstorage_free (&storage);
      assert (!live_allocations);
      fail_after = fault;
      ok = mulib_crawlqueue_push (&queue, "url");
      assert (ok ? queue.len == 1 : queue.len == 0);
      fail_after = SIZE_MAX;
      mulib_crawlqueue_free (&queue);
      assert (!live_allocations);
    }
  mulib_cookiejar_t jar = { 0 };
  mulib_webstorage_t storage = { 0 };
  assert (mulib_cookiejar_set (&jar, "x", "old"));
  assert (mulib_webstorage_set (&storage, "x", "old"));
  fail_after = 0;
  assert (!mulib_cookiejar_set (&jar, "x", "new"));
  assert (!mulib_webstorage_set (&storage, "x", "new"));
  assert (!strcmp (mulib_cookiejar_get (&jar, "x"), "old"));
  assert (!strcmp (mulib_webstorage_get (&storage, "x"), "old"));
  fail_after = SIZE_MAX;
  mulib_cookiejar_free (&jar);
  mulib_webstorage_free (&storage);
  for (size_t fault = 0; fault < 2; ++fault)
    {
      mulib_scrape_rule_t rule = { 0 };
      assert (mulib_scrape_rule_init (&rule, "old", "href", 0));
      fail_after = fault;
      assert (!mulib_scrape_rule_init (&rule, "new", "src", 1));
      assert (!strcmp (rule.selector, "old")
              && !strcmp (rule.attribute, "href") && !rule.all);
      fail_after = SIZE_MAX;
      mulib_scrape_rule_free (&rule);
    }
  mulib_browserlocator_t locator = { 0 };
  mulib_browsersnapshot_t snapshot = { 0 };
  mulib_weblocation_t location = { 0 };
  assert (
      mulib_browserlocator_init (&locator, MULIB_BROWSERLOCATOR_CSS, "old"));
  assert (mulib_browsersnapshot_create (&snapshot, "old", 3));
  assert (mulib_weblocation_set (&location, "http://old.example"));
  fail_after = 0;
  assert (
      !mulib_browserlocator_init (&locator, MULIB_BROWSERLOCATOR_TEXT, "new"));
  assert (!mulib_browsersnapshot_create (&snapshot, "new", 3));
  assert (!mulib_weblocation_set (&location, "http://new.example"));
  assert (!strcmp (locator.selector, "old")
          && locator.kind == MULIB_BROWSERLOCATOR_CSS);
  assert (!memcmp (snapshot.data, "old", 3));
  assert (!strcmp (location.href, "http://old.example"));
  fail_after = SIZE_MAX;
  mulib_browserlocator_free (&locator);
  mulib_browsersnapshot_free (&snapshot);
  mulib_weblocation_free (&location);
  assert (!live_allocations);
}

static void
input_boundaries (void)
{
  char output[64];
  assert (
      mulib_scrape_readable ("<!DOCTYPE html><p>x</p>", output, sizeof output)
      == 1);
  assert (!strcmp (output, "x"));
  assert (mulib_scrape_readable ("1 <3 > 2", output, sizeof output) == 8);
  assert (!strcmp (output, "1 <3 > 2"));
  assert (mulib_scrape_table_cells ("<!DOCTYPE html PUBLIC '<td>'><td>x")
          == 1);
  char text[8194];
  memset (text, 'x', 8193);
  text[8193] = 0;
  mulib_browserlocator_t locator = { 0 };
  mulib_scrape_rule_t rule = { 0 };
  mulib_crawlqueue_t queue = { 0 };
  assert (
      !mulib_browserlocator_init (&locator, MULIB_BROWSERLOCATOR_CSS, text));
  assert (!mulib_scrape_rule_init (&rule, text, NULL, 0));
  assert (!mulib_crawlqueue_push (&queue, text));
  text[8192] = 0;
  assert (
      mulib_browserlocator_init (&locator, MULIB_BROWSERLOCATOR_CSS, text));
  assert (mulib_scrape_rule_init (&rule, text, NULL, 0));
  assert (mulib_crawlqueue_push (&queue, text) == 1);
  mulib_browserlocator_free (&locator);
  mulib_scrape_rule_free (&rule);
  mulib_crawlqueue_free (&queue);

  /* Exercise every short prefix as its own NUL-terminated allocation. ASan
   * catches speculative reads past a terminator, even for malformed tags. */
  const char *samples[] = { "text/html; charset",
                            "text/html; charset=\"UTF-8\"",
                            "<a href='x > y'>",
                            "<meta name='x' content='y'>",
                            "<SCRIPT type='application/ld+json'>{}</SCRIPT>",
                            "<script><td>hidden</script>",
                            "<!--comment--><td>x</td>",
                            "http://[2001:db8::1]:443/?a#b" };
  for (size_t i = 0; i < sizeof samples / sizeof samples[0]; ++i)
    for (size_t n = 0; n <= strlen (samples[i]); ++n)
      {
        char *input = web_malloc (n + 1);
        assert (input);
        memcpy (input, samples[i], n);
        input[n] = 0;
        char out[256];
        mulib_webextract_span_t span = { NULL, 0 };
        size_t pos = 0;
        (void)mulib_charset_extract (input, out, sizeof out);
        (void)mulib_mime_content_type (input, out, sizeof out);
        (void)mulib_cookie_pair (input, out, 64, out + 64, 64);
        (void)mulib_urlcanon_normalize (input, out, sizeof out);
        (void)mulib_netpolicy_public_url (input);
        (void)mulib_scrape_metadata_get (input, "x", out, sizeof out);
        (void)mulib_search_serp_href (input, 0, out, sizeof out);
        (void)mulib_webextract_jsonld (input, &span);
        (void)mulib_scrape_table_cells (input);
        (void)mulib_scrape_readable (input, out, sizeof out);
        (void)mulib_sitemap_next (input, n, &pos, out, sizeof out);
        web_free (input);
      }
  for (size_t fault = 0; fault < 5; ++fault)
    {
      mulib_cookiejar_t jar = { 0 };
      mulib_webstorage_t storage = { 0 };
      char key[16];
      for (size_t i = 0; i < 8; ++i)
        {
          snprintf (key, sizeof key, "k%zu", i);
          assert (mulib_cookiejar_set (&jar, key, "old"));
          assert (mulib_webstorage_set (&storage, key, "old"));
        }
      fail_after = fault;
      int ok = mulib_cookiejar_set (&jar, "new", "new");
      assert (jar.length == (ok ? 9u : 8u));
      fail_after = SIZE_MAX;
      fail_after = fault;
      ok = mulib_webstorage_set (&storage, "new", "new");
      assert (storage.len == (ok ? 9u : 8u));
      fail_after = SIZE_MAX;
      for (size_t i = 0; i < 8; ++i)
        {
          snprintf (key, sizeof key, "k%zu", i);
          assert (!strcmp (mulib_cookiejar_get (&jar, key), "old"));
          assert (!strcmp (mulib_webstorage_get (&storage, key), "old"));
        }
      mulib_cookiejar_free (&jar);
      mulib_webstorage_free (&storage);
    }
  assert (!live_allocations);
}

int
main (void)
{
  urls ();
  headers ();
  extraction ();
  rules_and_formatting ();
  state_and_clocks ();
  ownership ();
  allocation_failures ();
  input_boundaries ();
  puts ("Web additions: boundary, integration and allocation-failure checks "
        "passed.");
  return 0;
}
