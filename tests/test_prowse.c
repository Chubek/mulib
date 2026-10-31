#include "../examples/prowse-common.h"

typedef struct fixture
{
  mulib_buffer_t input, output;
  size_t position, response;
  const char **responses;
  size_t response_count;
  int opens, closes, fail_read;
  unsigned http_status;
  char last_target[2048], last_method[16];
  mulib_json_t *last_body;
} fixture;
static int open_io (void *user, const char *host, unsigned port, int tls, unsigned timeout)
{
  fixture *f = (fixture *)user;
  assert (host && port && !tls && timeout); ++f->opens;
  if (f->responses)
    {
      char header[256]; const char *body;
      assert (f->response < f->response_count); body = f->responses[f->response++];
      f->input.length = f->position = f->output.length = 0;
      (void)snprintf (header, sizeof (header), "HTTP/1.1 %u Reply\r\nContent-Length: %zu\r\nContent-Type: application/json\r\n\r\n", f->http_status ? f->http_status : 200, strlen (body));
      assert (mulib_buffer_append (&f->input, header, strlen (header)));
      assert (mulib_buffer_append (&f->input, body, strlen (body)));
    }
  return 0;
}
static int read_io (void *user, void *out, size_t cap, size_t *count, unsigned timeout)
{
  fixture *f = (fixture *)user;
  size_t n = f->input.length - f->position;
  assert (timeout);
  if (f->fail_read) return MULIB_PROWSE_TIMEOUT;
  if (!n) return MULIB_PROWSE_CLOSED;
  if (n > cap) n = cap;
  if (n > 3) n = 3;
  memcpy (out, f->input.data + f->position, n); f->position += n; *count = n; return 0;
}
static int write_io (void *user, const void *data, size_t n, size_t *count, unsigned timeout)
{
  fixture *f = (fixture *)user;
  assert (timeout); if (n > 5) n = 5;
  assert (mulib_buffer_append (&f->output, data, n)); *count = n; return 0;
}
static void close_io (void *user)
{
  fixture *f = (fixture *)user;
  ++f->closes;
  if (f->responses)
    {
      mulib_http_message_t request = { 0 }; size_t used;
      assert (mulib_http_parse (f->output.data, f->output.length, 0, NULL, &request, &used) == MULIB_HTTP_OK);
      assert (used == f->output.length);
      (void)snprintf (f->last_target, sizeof (f->last_target), "%s", request.target);
      (void)snprintf (f->last_method, sizeof (f->last_method), "%s", request.method);
      mulib_json_destroy (f->last_body); f->last_body = request.body_length ? mulib_json_parse ((char *)request.body, request.body_length, NULL) : NULL;
      mulib_http_message_free (&request);
    }
}
static int random_io (void *user, void *out, size_t n)
{ (void)user; assert (n <= 16); memcpy (out, "the sample nonce", n); return 0; }
static mulib_prowse_io_t adapter (fixture *f)
{ mulib_prowse_io_t io = { f, open_io, read_io, write_io, close_io, random_io }; return io; }
static void fixture_free (fixture *f)
{ mulib_buffer_free (&f->input); mulib_buffer_free (&f->output); mulib_json_destroy (f->last_body); }
static void frame (fixture *f, unsigned opcode, int final, const char *text)
{
  size_t n = strlen (text); unsigned char header[4]; size_t h = 2;
  header[0] = (unsigned char)(opcode | (final ? 128 : 0));
  if (n < 126) header[1] = (unsigned char)n;
  else { header[1] = 126; header[2] = (unsigned char)(n >> 8); header[3] = (unsigned char)n; h = 4; }
  assert (mulib_buffer_append (&f->input, header, h)); assert (mulib_buffer_append (&f->input, text, n));
}
static void ws_hello (fixture *f)
{
  const char *text = "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
  assert (mulib_buffer_append (&f->input, text, strlen (text)));
}
static void cdp (void)
{
  fixture f = { 0 }; mulib_prowse_io_t io = adapter (&f);
  mulib_prowse_protocol_t *p; mulib_prowse_reply_t r = { 0 }; mulib_json_t *event = NULL;
  int error;
  ws_hello (&f); frame (&f, 1, 1, "{\"method\":\"Page.loadEventFired\",\"params\":{}}");
  frame (&f, 1, 0, "{\"id\":1,"); frame (&f, 9, 1, "ping"); frame (&f, 0, 1, "\"result\":{\"answer\":42}}");
  frame (&f, 1, 1, "{\"id\":2,\"error\":{\"code\":-1,\"message\":\"bad method\"}}");
  frame (&f, 1, 1, "{\"id\":99,\"result\":{}}");
  p = mulib_prowse_protocol_connect (MULIB_PROWSE_CDP, "ws://localhost:9222/devtools/page/test", NULL, &io, &error); assert (p && !error);
  assert (!mulib_prowse_protocol_call (p, "Example.answer", NULL, &r));
  assert (mulib_json_numeric (mulib_json_get (r.value, "answer")) == 42); mulib_prowse_reply_free (&r);
  assert (!mulib_prowse_protocol_event (p, &event)); assert (mulib_json_get (event, "method")); mulib_json_destroy (event);
  assert (mulib_prowse_protocol_call (p, "Bad.method", NULL, &r) == MULIB_PROWSE_REMOTE && r.remote_error); mulib_prowse_reply_free (&r);
  assert (mulib_prowse_protocol_call (p, "Wrong.id", NULL, &r) == MULIB_PROWSE_FORMAT && !r.json);
  assert (mulib_prowse_protocol_call (p, "Again", NULL, &r) == MULIB_PROWSE_CLOSED);
  assert (f.closes == 1);
  /* The client mask is mandatory, including its pong response. */
  {
    size_t at = 0, messages = 0;
    while (at + 4 <= f.output.length && memcmp (f.output.data + at, "\r\n\r\n", 4)) ++at;
    at += 4;
    while (at < f.output.length)
      {
        unsigned opcode = f.output.data[at] & 15; size_t n = f.output.data[at + 1] & 127;
        unsigned char decoded[256];
        assert (f.output.data[at + 1] & 128); assert (n < 126 && n < sizeof (decoded));
        for (size_t i = 0; i < n; ++i) decoded[i] = f.output.data[at + 6 + i] ^ f.output.data[at + 2 + i % 4];
        if (opcode == 10) assert (n == 4 && !memcmp (decoded, "ping", 4));
        else { mulib_json_t *json = mulib_json_parse ((char *)decoded, n, NULL); assert (json && mulib_json_get (json, "id")); mulib_json_destroy (json); ++messages; }
        at += 6 + n;
      }
    assert (messages == 3 && at == f.output.length);
  }
  mulib_prowse_protocol_destroy (p); fixture_free (&f);
}
static void marionette (void)
{
  fixture f = { 0 }; mulib_prowse_io_t io = adapter (&f); int error;
  mulib_prowse_protocol_t *p; mulib_prowse_reply_t reply = { 0 };
  const char *messages[] = { "{\"applicationType\":\"gecko\",\"marionetteProtocol\":3}", "[1,1,null,{\"value\":42}]" };
  for (size_t i = 0; i < 2; ++i)
    { char prefix[32]; int n = snprintf (prefix, sizeof (prefix), "%zu:", strlen (messages[i])); assert (mulib_buffer_append (&f.input, prefix, (size_t)n)); assert (mulib_buffer_append (&f.input, messages[i], strlen (messages[i]))); }
  p = mulib_prowse_protocol_connect (MULIB_PROWSE_MARIONETTE, "marionette://127.0.0.1:2828", NULL, &io, &error); assert (p);
  assert (!mulib_prowse_protocol_call (p, "WebDriver:ExecuteScript", NULL, &reply));
  assert (mulib_json_numeric (mulib_json_get (reply.value, "value")) == 42); mulib_prowse_reply_free (&reply);
  f.fail_read = 1; assert (mulib_prowse_protocol_call (p, "Timeout", NULL, &reply) == MULIB_PROWSE_TIMEOUT);
  assert (f.closes == 1); mulib_prowse_protocol_destroy (p); fixture_free (&f);
}
static void webdriver (void)
{
  const char *responses[] = {
    "{\"value\":{\"sessionId\":\"s/1\",\"capabilities\":{}}}", "{\"value\":null}", "{\"value\":42}",
    "{\"value\":\"hello\"}", "{\"value\":{\"element-6066-11e4-a52e-4f735466cecf\":\"element\"}}", "{\"value\":null}",
    "{\"value\":{\"element-6066-11e4-a52e-4f735466cecf\":\"element\"}}", "{\"value\":null}",
    "{\"value\":\"cG5n\"}", "{\"value\":{\"error\":\"data\",\"message\":\"text\"}}", "{\"value\":null}", "{\"value\":{\"error\":\"unknown command\",\"message\":\"missing\"}}"
  };
  fixture f = { 0 }; mulib_prowse_io_t io = adapter (&f); int error;
  mulib_prowse_protocol_t *p; mulib_prowse_control_t *c; mulib_json_t *value = NULL; mulib_prowse_reply_t reply = { 0 };
  f.responses = responses; f.response_count = sizeof (responses) / sizeof (*responses);
  p = mulib_prowse_protocol_connect (MULIB_PROWSE_WEBDRIVER, "http://localhost:4444/wd/hub", NULL, &io, &error); assert (p);
  c = mulib_prowse_control_create (p, NULL); assert (c);
  assert (!mulib_prowse_control_start (c, NULL)); assert (!strcmp (f.last_target, "/wd/hub/session"));
  assert (mulib_json_get (mulib_json_get (f.last_body, "capabilities"), "alwaysMatch"));
  assert (!mulib_prowse_control_navigate (c, "https://example.test/?q=\"value\"")); assert (!strcmp (f.last_target, "/wd/hub/session/s%2F1/url"));
  assert (!mulib_prowse_control_evaluate (c, "42", &value)); assert (mulib_json_numeric (value) == 42); mulib_json_destroy (value); value = NULL;
  assert (!mulib_prowse_control_query (c, "h1[data-x=\"a\"]", "text", NULL, 0, &value));
  assert (!strcmp (mulib_json_text (value, NULL), "hello")); mulib_json_destroy (value); value = NULL;
  assert (strstr (mulib_json_text (mulib_json_get (f.last_body, "script"), NULL), "h1[data-x=\\\"a\\\"]"));
  assert (!mulib_prowse_control_click (c, "button")); assert (strstr (f.last_target, "/element/element/click"));
  assert (!mulib_prowse_control_type (c, "input", "abc")); assert (!strcmp (mulib_json_text (mulib_json_get (f.last_body, "text"), NULL), "abc"));
  assert (!mulib_prowse_control_screenshot (c, &value)); assert (mulib_json_typeof (value) == MULIB_JSON_STRING); mulib_json_destroy (value); value = NULL;
  assert (!mulib_prowse_control_evaluate (c, "({error:'data',message:'text'})", &value));
  assert (!strcmp (mulib_json_text (mulib_json_get (value, "error"), NULL), "data")); mulib_json_destroy (value);
  assert (!mulib_prowse_control_quit (c)); assert (!strcmp (f.last_method, "DELETE"));
  f.http_status = 404;
  assert (mulib_prowse_protocol_http (p, "GET", "/missing", NULL, &reply) == MULIB_PROWSE_REMOTE);
  mulib_prowse_reply_free (&reply); assert (f.opens == f.closes && f.response == f.response_count);
  mulib_prowse_control_destroy (c); mulib_prowse_protocol_destroy (p); fixture_free (&f);
}
static void lua_dsl (void)
{
  mulib_prowse_control_t *c = mulib_prowse_control_create (NULL, NULL);
  mulib_mune_result_t *result = NULL; mulib_mune_error_t error;
  const char *source =
    "local p=require('lprowse'); assert(p==browser); assert(p.json({x=p.array(),y=p.null})=='{\"x\":[],\"y\":null}'); "
    "local cycle={}; cycle.x=cycle; assert(not pcall(p.json,cycle)); assert(not pcall(p.go,'https://example.test')); "
    "local seen={}; p.go=function(url) seen[#seen+1]=url; return p end; p.url=function() return seen[#seen] end; "
    "p.title=function() return 'title' end; p.links=function() return {'http://example.test/a#one','http://example.test/a#two','http://elsewhere.test/'} end; "
    "local rows,stats=p.crawl('http://example.test/',{max_pages=10,max_depth=3}); assert(#rows==2 and stats.visited==2 and stats.queued==2); "
    "local n=0; assert(p.retry(3,0,function() n=n+1; if n<3 then error('again') end; return 42 end)==42); "
    "p.text=function(s) return s end; local row=p.scrape({a='h1',b=function() return 7 end}); assert(row.a=='h1' and row.b==7)";
  int rc = mulib_prowse_control_run (c, source, strlen (source), &result, &error);
  if (rc) fprintf (stderr, "%s\n", error.message);
  assert (!rc); mulib_mune_result_destroy (result); mulib_prowse_control_destroy (c);
}
static void wire_errors (void)
{
  static const char *const invalid[] = {
    "[]", "{}", "{\"id\":1,\"result\":null}", "{\"id\":1.5,\"result\":{}}",
    "{\"id\":1,\"result\":{},\"error\":{}}", "{\"id\":1,\"result\":{},\"method\":\"event\"}",
    "{\"id\":1,\"error\":{\"message\":\"missing code\"}}",
    "{\"id\":1,\"error\":{\"code\":1.5,\"message\":\"fractional code\"}}",
    "{\"id\":1,\"result\":{},\"sessionId\":\"wrong\"}",
    "{\"method\":\"event\",\"result\":{}}", "{\"method\":\"event\",\"params\":null}",
    "{\"method\":\"event\",\"sessionId\":\"bad\\u0000id\"}"
  };
  static const unsigned char invalid_frames[][12] = {
    { 0x81, 0x80, 0, 0, 0, 0 }, /* masked server frame */
    { 0xc1, 0 },                /* unnegotiated extension */
    { 0x82, 0 },                /* binary */
    { 0x80, 0 },                /* orphan continuation */
    { 0x89, 126, 0, 126 },      /* oversized control */
    { 0x81, 126, 0, 1, 'x' },   /* noncanonical length */
    { 0x81, 127, 0x80, 0, 0, 0, 0, 0, 0, 0 },
    { 0x88, 2, 3, 237 },        /* reserved close code 1005 */
    { 0x88, 1, 0 },             /* incomplete close code */
    { 0x81, 1, 0xff },          /* invalid text UTF-8 */
    { 0x81, 126, 4, 0 }         /* declared message over quota */
  };
  for (size_t i = 0; i < sizeof (invalid) / sizeof (*invalid) + sizeof (invalid_frames) / sizeof (*invalid_frames); ++i)
    {
      fixture f = { 0 }; mulib_prowse_io_t io = adapter (&f);
      mulib_prowse_protocol_options_t options = { 256, 1, 16, 1000 };
      mulib_prowse_protocol_t *p; mulib_prowse_reply_t r = { 0 }; int rc;
      ws_hello (&f);
      if (i < sizeof (invalid) / sizeof (*invalid)) frame (&f, 1, 1, invalid[i]);
      else assert (mulib_buffer_append (&f.input, invalid_frames[i - sizeof (invalid) / sizeof (*invalid)], sizeof (*invalid_frames)));
      p = mulib_prowse_protocol_connect (MULIB_PROWSE_CDP, "ws://127.0.0.1:9222/", &options, &io, &rc); assert (p);
      rc = mulib_prowse_protocol_call (p, "Test", NULL, &r);
      assert ((rc == MULIB_PROWSE_FORMAT || rc == MULIB_PROWSE_LIMIT) && !r.json && f.closes == 1);
      mulib_prowse_protocol_destroy (p); fixture_free (&f);
    }
  {
    fixture f = { 0 }; mulib_prowse_io_t io = adapter (&f);
    mulib_prowse_protocol_options_t options = { 256, 1, 16, 1000 };
    mulib_prowse_protocol_t *p; mulib_prowse_reply_t r = { 0 }; mulib_json_t *event = NULL; int rc;
    ws_hello (&f); frame (&f, 1, 1, "{\"method\":\"first\"}"); frame (&f, 1, 1, "{\"method\":\"last\"}");
    frame (&f, 1, 1, "{\"id\":1,\"result\":{},\"sessionId\":\"page\"}");
    p = mulib_prowse_protocol_connect (MULIB_PROWSE_CDP, "ws://127.0.0.1:9222/", &options, &io, &rc); assert (p);
    assert (!mulib_prowse_protocol_session (p, "page")); assert (!mulib_prowse_protocol_call (p, "Test", NULL, &r));
    assert (mulib_prowse_protocol_dropped (p) == 1 && !mulib_prowse_protocol_event (p, &event));
    assert (!strcmp (mulib_json_text (mulib_json_get (event, "method"), NULL), "last"));
    mulib_json_destroy (event); mulib_prowse_reply_free (&r); mulib_prowse_protocol_destroy (p); fixture_free (&f);
  }
  for (size_t cut = 0; cut < 24; ++cut)
    {
      fixture f = { 0 }; mulib_prowse_io_t io = adapter (&f);
      mulib_prowse_protocol_t *p; mulib_prowse_reply_t r = { 0 }; size_t start; int rc;
      ws_hello (&f); start = f.input.length; frame (&f, 1, 1, "{\"id\":1,\"result\":{\"ok\":true}}");
      assert (f.input.length - start > cut); f.input.length = start + cut;
      p = mulib_prowse_protocol_connect (MULIB_PROWSE_CDP, "ws://127.0.0.1:9222/", NULL, &io, &rc); assert (p);
      assert (mulib_prowse_protocol_call (p, "Test", NULL, &r) == MULIB_PROWSE_CLOSED && !r.json);
      mulib_prowse_protocol_destroy (p); fixture_free (&f);
    }
}
static void marionette_control (void)
{
  static const char *const results[] = {
    "{\"sessionId\":\"test\",\"capabilities\":{}}", "{\"value\":42}",
    "{\"value\":{\"element-6066-11e4-a52e-4f735466cecf\":\"element\"}}", "{\"value\":null}",
    "{\"value\":\"cG5n\"}", "{\"value\":null}"
  };
  fixture f = { 0 }; mulib_prowse_io_t io = adapter (&f);
  mulib_prowse_protocol_t *p; mulib_prowse_control_t *c; mulib_json_t *value = NULL;
  mulib_json_t *caps = mulib_json_parse ("{\"acceptInsecureCerts\":true}", 28, NULL);
  const char *hello = "24:{\"marionetteProtocol\":3}";
  int rc;
  assert (caps); assert (mulib_buffer_append (&f.input, hello, strlen (hello)));
  for (size_t i = 0; i < sizeof (results) / sizeof (*results); ++i)
    {
      char body[512], prefix[32];
      int n = snprintf (body, sizeof (body), "[1,%zu,null,%s]", i + 1, results[i]);
      int m = snprintf (prefix, sizeof (prefix), "%d:", n);
      assert (mulib_buffer_append (&f.input, prefix, (size_t)m)); assert (mulib_buffer_append (&f.input, body, (size_t)n));
    }
  p = mulib_prowse_protocol_connect (MULIB_PROWSE_MARIONETTE, "marionette://127.0.0.1:2828", NULL, &io, &rc); assert (p);
  c = mulib_prowse_control_create (p, NULL); assert (c);
  assert (!mulib_prowse_control_start (c, caps));
  {
    size_t i = 0;
    mulib_json_t *request;
    while (i < f.output.length && f.output.data[i] != ':') ++i;
    request = mulib_json_parse ((char *)f.output.data + i + 1, f.output.length - i - 1, NULL); assert (request);
    assert (mulib_json_boolean (mulib_json_get (mulib_json_at (request, 3), "acceptInsecureCerts")));
    assert (!mulib_json_get (mulib_json_at (request, 3), "capabilities")); mulib_json_destroy (request);
  }
  assert (!mulib_prowse_control_evaluate (c, "42", &value) && mulib_json_numeric (value) == 42); mulib_json_destroy (value); value = NULL;
  assert (!mulib_prowse_control_click (c, "button"));
  assert (!mulib_prowse_control_screenshot (c, &value) && mulib_json_typeof (value) == MULIB_JSON_STRING); mulib_json_destroy (value);
  assert (!mulib_prowse_control_quit (c)); mulib_prowse_control_disconnect (c);
  mulib_prowse_control_destroy (c); mulib_prowse_protocol_destroy (p); mulib_json_destroy (caps); fixture_free (&f);
}
int main (int argc, char **argv)
{
  if (argc == 2 && !strcmp (argv[1], "--repl"))
    {
      mulib_prowse_control_t *c = mulib_prowse_control_create (NULL, NULL);
      int rc;
      assert (c); rc = mulib_prowse_control_repl (c, 0, 1); mulib_prowse_control_destroy (c); return rc != 0;
    }
  if (argc == 3)
    {
      mulib_prowse_protocol_kind_t kind = !strcmp (argv[1], "--cdp") ? MULIB_PROWSE_CDP : MULIB_PROWSE_WEBDRIVER;
      mulib_prowse_protocol_t *p;
      mulib_prowse_reply_t reply = { 0 };
      int error;
      p = mulib_prowse_protocol_connect (kind, argv[2], NULL, NULL, &error); assert (p);
      error = kind == MULIB_PROWSE_CDP ? mulib_prowse_protocol_call (p, "Browser.getVersion", NULL, &reply)
          : mulib_prowse_protocol_http (p, "GET", "/status", NULL, &reply);
      assert (!error && mulib_json_boolean (mulib_json_get (reply.value, "ready")));
      mulib_prowse_reply_free (&reply); mulib_prowse_protocol_destroy (p); return 0;
    }
  cdp (); marionette (); webdriver (); lua_dsl (); wire_errors (); marionette_control ();
  puts ("prowse: fragmented transports, correlation, errors, control mapping and Lua DSL passed"); return 0;
}
