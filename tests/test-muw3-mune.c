#include "../library/microchan/runtime.c"
#include "../library/lmuw3/runtime.c"
#include <assert.h>
static int mock_http (void *user, const mulib_uri_t *u, const void *p, size_t n,
    mulib_webclient_receive_fn receive, void *receiver)
{
  mulib_http_message_t request = {0};
  size_t used;
  static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 31\r\n\r\n{\"ok\":true,\"items\":[],\"x\":null}";
  static const char binary[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na\0b";
  (void)user; (void)u;
  assert (mulib_http_parse (p, n, 0, NULL, &request, &used) == 1);
  if (!strcmp (request.target, "/binary")) {
    assert (request.body_length == 3 && !memcmp (request.body, "x\0y", 3));
    mulib_http_message_free (&request); return receive (receiver, binary, sizeof binary - 1);
  }
  assert (!strcmp (request.target, "/api?name=a+b") && !strcmp (request.method, "POST"));
  assert (!strcmp (mulib_http_header (&request, "Authorization"), "Bearer token"));
  assert (!strcmp (mulib_http_header (&request, "Content-Type"), "application/json"));
  assert (request.body_length == 7 && !memcmp (request.body, "{\"x\":1}", 7));
  mulib_http_message_free (&request); return receive (receiver, reply, sizeof reply - 1);
}
int main (void)
{
  const char script[] =
    "local w = require('lmuw3'); assert(w == muw3)\n"
    "local r = w.request{url='http://unit.test/api', method='POST', query={name='a b'}, json={x=1}, bearer='token'}\n"
    "r:expect(200); assert(r:header('content-type') == 'application/json')\n"
    "local j = r:json(); assert(j.ok and j.x == w.null and w.json_encode(j.items) == '[]')\n"
    "assert(w.json_decode(w.json_encode({a='\\0', b=w.array()})).a == '\\0')\n"
    "local y = w.yaml_decode('answer: 42\\nlist: []\\n'); assert(y.answer == 42)\n"
    "assert(w.yaml_decode(w.yaml_encode(y)).answer == 42)\n"
    "assert(w.form({q='a+b'}) == 'q=a%2Bb')\n"
    "assert(w.resolve('http://unit.test/a/b', '../c') == 'http://unit.test/c')\n"
    "local b = w.post('http://unit.test/binary', 'x\\0y'); assert(b.body == 'a\\0b')\n"
    "assert(not pcall(w.json_decode, '{'))\n"
    "assert(not pcall(w.json_encode, {bad=string.char(255)}))\n"
    "assert(not pcall(w.request, {url='http://unit.test/', headers={{'X\\r\\nOops','yes'}}}))\n"
    "local cycle = {}; cycle.x = cycle; assert(not pcall(w.json_encode, cycle))\n"
    "assert(not pcall(w.request, {url='http://unit.test/', body='a', json={}}))\n"
    "saved = (saved or 0) + 1; return saved\n";
  mulib_muw3_client_t client = mulib_muw3_client_default ();
  mulib_muw3_engine_t *e;
  mulib_mune_result_t *result = NULL;
  mulib_mune_error_t error;
  client.http = mock_http;
  e = mulib_muw3_engine_create (&client, NULL, &error);
  assert (e);
  for (int mode = 0; mode < 2; ++mode) {
    int rc;
    assert (!mulib_mune_set_mode (mulib_muw3_engine_lua (e), (mulib_mune_mode_t)mode));
    rc = mulib_muw3_engine_run (e, script, sizeof script - 1, &result, &error);
    if (rc) fprintf (stderr, "Weblet %zu: %s\n", error.offset, error.message);
    assert (!rc && mulib_mune_result_get (result, 0).as.integer == mode + 1);
    mulib_mune_result_destroy (result); result = NULL;
    (void)mulib_mune_collect (mulib_muw3_engine_lua (e));
  }
  mulib_muw3_engine_destroy (e);
  puts ("MuW3 Mune: HTTP, binary strings, JSON/YAML, errors, GC and persistent state passed");
  return 0;
}
