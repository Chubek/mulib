#include "mulib-graphql.h"
#include "mulib-jsonrpc.h"
#include "mulib-rest.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENDPOINT "http://fixture.test/api"

typedef struct fixture {
  const char *wire_method, *accept, *content_type;
  const char *reply, *target;
  unsigned status, calls;
  int fail;
  mulib_json_t *sent;
} fixture_t;

static int exchange(void *user, const mulib_uri_t *uri, const void *wire, size_t n,
                    mulib_webclient_receive_fn receive, void *receiver) {
  fixture_t *f = (fixture_t *)user;
  mulib_http_message_t req = {0};
  size_t used = 0, cursor = 0, length = f->reply ? strlen(f->reply) : 0;
  const char *value;
  char header[128];
  int bytes;
  ++f->calls;
  assert(uri->host.length == 12 && memcmp(uri->host.data, "fixture.test", 12) == 0);
  assert(mulib_http_parse(wire, n, 0, NULL, &req, &used) == MULIB_HTTP_OK);
  assert(used == n && strcmp(req.method, f->wire_method) == 0 &&
         strcmp(req.target, f->target ? f->target : "/api") == 0);
  value = mulib_http_header_next(&req, "accept", &cursor);
  assert(value && strcmp(value, f->accept) == 0 &&
         !mulib_http_header_next(&req, "accept", &cursor));
  cursor = 0;
  value = mulib_http_header_next(&req, "content-type", &cursor);
  if (f->content_type) {
    assert(value && strcmp(value, f->content_type) == 0 &&
           !mulib_http_header_next(&req, "content-type", &cursor));
  } else assert(!value);
  mulib_json_destroy(f->sent);
  f->sent = NULL;
  if (req.body_length) {
    f->sent = mulib_json_parse((const char *)req.body, req.body_length, NULL);
    assert(f->sent);
  }
  mulib_http_message_free(&req);
  if (f->fail) return 0;
  if (f->status == 204) {
    static const char no_content[] = "HTTP/1.1 204 No Content\r\n\r\n";
    assert(!length);
    return receive(receiver, no_content, sizeof no_content - 1);
  }
  bytes = snprintf(header, sizeof header, "HTTP/1.1 %u Test\r\nContent-Length: %zu\r\n\r\n",
                   f->status, length);
  assert(bytes > 0 && (size_t)bytes < sizeof header);
  return receive(receiver, header, (size_t)bytes) &&
         (!length || receive(receiver, f->reply, length));
}

static mulib_json_t *json(const char *text) {
  mulib_json_t *value = mulib_json_parse(text, strlen(text), NULL);
  assert(value);
  return value;
}

static void text_is(const mulib_json_t *node, const char *expected) {
  size_t n = 0;
  const char *text = mulib_json_text(node, &n);
  assert(text && n == strlen(expected) && memcmp(text, expected, n) == 0);
}

static void test_rest(mulib_muw3_client_t *client, fixture_t *f) {
  const mulib_http_header_t pair = {"a b", "%/?\xC3\xA9"};
  const mulib_http_header_t invalid = {"Invalid Name", "value"};
  const mulib_http_header_t custom[] = {
    {"aCCePt", "application/problem+json"}, {"cONTENT-TYPE", "application/vnd.test+json"}
  };
  const char expected[] = ENDPOINT "?x=1&a+b=%25%2F%3F%C3%A9#anchor";
  mulib_cdd_rest_response_t out = {0};
  mulib_json_t *body = json("{\"enabled\":true}"), *previous;
  char *url = mulib_cdd_rest_url_query(ENDPOINT "?x=1#anchor", &pair, 1, sizeof expected - 1);
  assert(url && strcmp(url, expected) == 0);
  assert(!mulib_cdd_rest_url_query(ENDPOINT "?x=1#anchor", &pair, 1, sizeof expected - 2));
  free(url);
  url = mulib_rest_url_query("/api?#hash", &pair, 1, 0);
  assert(url && strcmp(url, "/api?a+b=%25%2F%3F%C3%A9#hash") == 0);
  free(url);
  url = mulib_cdd_rest_url_query("/api?k=v&", &pair, 1, SIZE_MAX);
  assert(url && strcmp(url, "/api?k=v&a+b=%25%2F%3F%C3%A9") == 0);
  free(url);
  url = mulib_cdd_rest_url_query("/api#hash", NULL, 0, 9);
  assert(url && strcmp(url, "/api#hash") == 0);
  free(url);
  assert(!mulib_cdd_rest_url_query("/api#hash", NULL, 0, 8));
  assert(!mulib_cdd_rest_url_query("/bad%QQ", &pair, 1, 0));
  assert(!mulib_cdd_rest_url_query("/api", &pair, SIZE_MAX, 32));
  assert(!mulib_cdd_rest_url_query("/api", NULL, 1, 0));

  f->wire_method = "GET";
  f->accept = "application/json";
  f->status = 204;
  f->target = "/api?name=value";
  assert(mulib_cdd_rest_request(client, NULL, ENDPOINT "?name=value#local", NULL, 0,
                                NULL, 1, &out) == 1);
  assert(!out.json && out.http.message.status == 204 && !f->sent);
  mulib_cdd_rest_response_free(&out);
  f->target = NULL;

  f->wire_method = "PATCH";
  f->accept = custom[0].value;
  f->content_type = custom[1].value;
  f->status = 422;
  f->reply = "{\"detail\":\"invalid input\"}";
  assert(mulib_rest_request(client, "PATCH", ENDPOINT, custom, 2, body, 1, &out) == 1);
  assert(out.http.message.status == 422);
  assert(mulib_json_boolean(mulib_json_get(f->sent, "enabled")));
  text_is(mulib_json_get(out.json, "detail"), "invalid input");
  previous = out.json;
  f->reply = "null true";
  assert(mulib_cdd_rest_request(client, "PATCH", ENDPOINT, custom, 2, body, 1, &out) == -1);
  assert(out.json == previous && out.http.message.status == 422);
  f->fail = 1;
  assert(mulib_cdd_rest_request(client, "PATCH", ENDPOINT, custom, 2, body, 0, &out) ==
         MULIB_MUW3_TRANSPORT);
  f->fail = 0;
  assert(out.json == previous);
  mulib_rest_response_free(&out);
  assert(mulib_cdd_rest_request(client, "PATCH", ENDPOINT, custom, 2, body, 0, &out) == 1);
  assert(!out.json && out.http.message.body_length == strlen(f->reply));
  mulib_cdd_rest_response_free(&out);
  mulib_cdd_rest_response_free(&out);

  f->calls = 0;
  assert(mulib_cdd_rest_request(client, "GET", ENDPOINT, custom, SIZE_MAX, NULL, 0, &out) == -1);
  assert(mulib_cdd_rest_request(client, "GET", ENDPOINT, NULL, 0, NULL, 0, NULL) == -1);
  assert(mulib_cdd_rest_request(client, "GET", ENDPOINT, &invalid, 1, NULL, 0, &out) == -1);
  assert(mulib_cdd_rest_request(client, "NO SPACE", ENDPOINT, NULL, 0, NULL, 0, &out) == -1);
  client->options.web.http.max_body_bytes = 1;
  assert(mulib_cdd_rest_request(client, "PATCH", ENDPOINT, custom, 2, body, 0, &out) ==
         MULIB_MUW3_LIMIT);
  client->options.web.http.max_body_bytes = 0;
  assert(!f->calls);
  mulib_json_destroy(body);
}

static void test_graphql(mulib_muw3_client_t *client, fixture_t *f) {
  const char query[] = "query Read($name:String!){viewer(name:$name){id}}";
  const char *bad[] = {
    "[]", "{}", "null", "{\"data\":null}", "{\"data\":[]}",
    "{\"data\":{},\"unexpected\":true}", "{\"extensions\":{}}",
    "{\"data\":{},\"extensions\":[]}", "{\"data\":{},\"errors\":[]}",
    "{\"errors\":[null]}", "{\"errors\":[{\"message\":7}]}",
    "{\"errors\":[{\"message\":\"oops\",\"locations\":[{\"line\":1,\"column\":0}]}]}",
    "{\"errors\":[{\"message\":\"oops\",\"path\":[\"viewer\",-1]}]}"
  };
  mulib_cdd_graphql_result_t out = {0};
  mulib_json_t *variables = json("{\"name\":\"Ada\"}"), *previous;
  const mulib_http_header_t custom = {"aCcePT", "application/graphql-response+json"};
  f->wire_method = "POST";
  f->accept = "application/graphql-response+json, application/json;q=0.9";
  f->content_type = "application/json";
  f->status = 400;
  f->reply = "{\"data\":{\"viewer\":null},\"errors\":[{\"message\":\"field unavailable\","
             "\"locations\":[{\"line\":1,\"column\":5}],\"path\":[\"viewer\",0]}]}";
  assert(mulib_cdd_graphql_execute(client, ENDPOINT, query, variables, "Read", NULL, 0, &out) == 1);
  assert(out.has_errors && out.response.http.message.status == 400);
  text_is(mulib_json_get(f->sent, "query"), query);
  text_is(mulib_json_get(f->sent, "operationName"), "Read");
  text_is(mulib_json_get(mulib_json_get(f->sent, "variables"), "name"), "Ada");
  text_is(mulib_json_get(variables, "name"), "Ada");
  previous = out.response.json;
  for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) {
    f->reply = bad[i];
    assert(mulib_cdd_graphql_execute(client, ENDPOINT, query, variables, NULL, NULL, 0, &out) == -1);
    assert(out.response.json == previous && out.has_errors);
  }
  f->calls = 0;
  assert(mulib_cdd_graphql_execute(client, ENDPOINT, query, variables, "1Read", NULL, 0, &out) == -1);
  assert(mulib_cdd_graphql_execute(client, ENDPOINT, query, mulib_json_get(variables, "name"),
                                   NULL, NULL, 0, &out) == -1);
  assert(mulib_cdd_graphql_execute(client, ENDPOINT, query, NULL, NULL, NULL, 0, NULL) == -1);
  assert(!f->calls);
  mulib_cdd_graphql_result_free(&out);
  f->status = 200;
  f->accept = custom.value;
  f->reply = "{\"data\":{},\"extensions\":{\"version\":\"1\"}}";
  assert(mulib_graphql_execute(client, ENDPOINT, query, NULL, NULL, &custom, 1, &out) == 1);
  assert(!out.has_errors && !mulib_json_get(f->sent, "variables") &&
         !mulib_json_get(f->sent, "operationName"));
  mulib_graphql_result_free(&out);
  f->status = 400;
  f->reply = "{\"errors\":[{\"message\":\"Operation not found\"}]}";
  assert(mulib_cdd_graphql_execute(client, ENDPOINT, query, NULL, "Other", &custom, 1, &out) == 1);
  assert(out.has_errors && !mulib_json_get(out.response.json, "data"));
  mulib_cdd_graphql_result_free(&out);
  mulib_json_destroy(variables);
}

static void test_jsonrpc(mulib_muw3_client_t *client, fixture_t *f) {
  static const char response[] = "{\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000b\",\"result\":null}";
  const char *bad[] = {
    "[]", "{}", "{\"jsonrpc\":\"2.0\",\"result\":null}",
    "{\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000b\",\"result\":null,\"error\":null}",
    "{\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000b\",\"error\":{\"code\":1.5,\"message\":\"Oops\"}}",
    "{\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000b\",\"error\":{\"code\":-32601,\"message\":12}}",
    "{\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000c\",\"result\":null}"
  };
  const char *bad_requests[] = {
    "{}", "[]", "[null]", "{\"jsonrpc\":2.0,\"method\":\"add\"}",
    "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.reserved\"}",
    "{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"id\":false}",
    "{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"params\":null}"
  };
  mulib_cdd_jsonrpc_reply_t out = {0};
  mulib_json_t *id = mulib_json_string_n("a\0b", 3), *params = json("[1,2]"), *batch, *value;
  assert(id && params);
  f->wire_method = "POST";
  f->accept = "application/json";
  f->content_type = "application/json";
  f->status = 200;
  f->reply = response;
  assert(mulib_cdd_jsonrpc_call(client, ENDPOINT, "add", params, id, &out) == 1);
  assert(!out.remote_error && mulib_json_get(out.response.json, "result"));
  text_is(mulib_json_get(f->sent, "jsonrpc"), "2.0");
  text_is(mulib_json_get(f->sent, "method"), "add");
  assert(mulib_json_size(mulib_json_get(f->sent, "params")) == 2);
  value = out.response.json;
  for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) {
    f->reply = bad[i];
    assert(mulib_cdd_jsonrpc_call(client, ENDPOINT, "add", NULL, id, &out) == -1);
    assert(out.response.json == value);
  }
  f->status = 204;
  f->reply = NULL;
  assert(mulib_cdd_jsonrpc_call(client, ENDPOINT, "add", NULL, id, &out) == -1);
  assert(out.response.json == value);
  mulib_cdd_jsonrpc_reply_free(&out);
  assert(mulib_jsonrpc_call(client, ENDPOINT, "update", NULL, NULL, &out) == 1);
  assert(!out.response.json && !mulib_json_get(f->sent, "id"));
  mulib_jsonrpc_reply_free(&out);
  f->status = 200;
  f->reply = response;
  assert(mulib_cdd_jsonrpc_call(client, ENDPOINT, "update", NULL, NULL, &out) == -1);

  value = json("{\"jsonrpc\":\"2.0\",\"method\":\"read\",\"id\":\"different\"}");
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, value, NULL, 0, &out) == -1);
  mulib_json_destroy(value);
  value = mulib_json_new(MULIB_JSON_NULL);
  assert(value);
  f->reply = "{\"jsonrpc\":\"2.0\",\"id\":null,\"result\":1}";
  assert(mulib_cdd_jsonrpc_call(client, ENDPOINT, "read", NULL, value, &out) == 1);
  assert(mulib_json_get(f->sent, "id"));
  mulib_cdd_jsonrpc_reply_free(&out);
  mulib_json_destroy(value);

  batch = json("[{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"id\":1},"
               "{\"jsonrpc\":\"2.0\",\"method\":\"update\"},"
               "{\"jsonrpc\":\"2.0\",\"method\":\"read\",\"id\":2}]");
  f->reply = "[{\"jsonrpc\":\"2.0\",\"id\":2,\"error\":{\"code\":-32601,\"message\":\"Not found\"}},"
             "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":3}]";
  assert(mulib_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == 1);
  assert(out.remote_error && mulib_json_size(out.response.json) == 2);
  mulib_jsonrpc_reply_free(&out);
  f->reply = response;
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == -1);
  f->reply = "[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":3}]";
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == -1);
  f->reply = "[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":3},"
             "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":3}]";
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == -1);
  f->reply = "[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":3},"
             "{\"jsonrpc\":\"2.0\",\"id\":\"2\",\"result\":3}]";
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == -1);
  f->status = 204;
  f->reply = NULL;
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == -1);
  mulib_json_destroy(batch);
  batch = json("[{\"jsonrpc\":\"2.0\",\"method\":\"a\",\"id\":\"a\\u0000b\"},"
               "{\"jsonrpc\":\"2.0\",\"method\":\"b\",\"id\":null},"
               "{\"jsonrpc\":\"2.0\",\"method\":\"c\",\"id\":0}]");
  f->status = 200;
  f->reply = "[{\"jsonrpc\":\"2.0\",\"id\":null,\"result\":2},"
             "{\"jsonrpc\":\"2.0\",\"id\":-0.0,\"result\":3},"
             "{\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000b\",\"result\":1}]";
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == 1);
  assert(!out.remote_error && mulib_json_size(out.response.json) == 3);
  mulib_cdd_jsonrpc_reply_free(&out);
  mulib_json_destroy(batch);

  batch = json("[{\"jsonrpc\":\"2.0\",\"method\":\"update\"},{\"jsonrpc\":\"2.0\",\"method\":\"reset\"}]");
  f->status = 204;
  f->reply = NULL;
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == 1);
  assert(!out.response.json && !out.remote_error);
  mulib_cdd_jsonrpc_reply_free(&out);
  mulib_json_destroy(batch);

  f->calls = 0;
  for (size_t i = 0; i < sizeof bad_requests / sizeof *bad_requests; ++i) {
    value = json(bad_requests[i]);
    assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, value, NULL, 0, &out) == -1);
    mulib_json_destroy(value);
  }
  assert(!mulib_jsonrpc_make("rpc.reserved", NULL, id));
  assert(!mulib_cdd_jsonrpc_make("add", id, NULL));
  assert(mulib_cdd_jsonrpc_call(client, ENDPOINT, "add", NULL, id, NULL) == -1);
  batch = json("[{\"jsonrpc\":\"2.0\",\"method\":\"read\",\"id\":0},"
               "{\"jsonrpc\":\"2.0\",\"method\":\"read\",\"id\":-0.0}]");
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == -1);
  mulib_json_destroy(batch);
  assert(!f->calls);

  batch = mulib_json_new(MULIB_JSON_ARRAY);
  assert(batch);
  for (size_t i = 0; i < 1024; ++i) {
    value = mulib_cdd_jsonrpc_make("update", NULL, NULL);
    assert(value && mulib_json_append(batch, value));
  }
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == 1);
  mulib_cdd_jsonrpc_reply_free(&out);
  value = mulib_cdd_jsonrpc_make("update", NULL, NULL);
  assert(value && mulib_json_append(batch, value));
  assert(mulib_cdd_jsonrpc_exchange(client, ENDPOINT, batch, NULL, 0, &out) == MULIB_MUW3_LIMIT);
  assert(f->calls == 1);
  mulib_json_destroy(batch);

  mulib_cdd_jsonrpc_reply_free(NULL);
  mulib_json_destroy(params);
  mulib_json_destroy(id);
}

int main(void) {
  fixture_t fixture = {0};
  mulib_muw3_client_t client = mulib_muw3_client_default();
  client.http = exchange;
  client.http_user = &fixture;
  test_rest(&client, &fixture);
  test_graphql(&client, &fixture);
  test_jsonrpc(&client, &fixture);
  mulib_json_destroy(fixture.sent);
  puts("REST, GraphQL, JSON-RPC: offline HTTP checks passed");
  return 0;
}
