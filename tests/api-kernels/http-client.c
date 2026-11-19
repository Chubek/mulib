/* All network I/O below targets a fixture bound to TCP loopback. */
#include "mulib-graphql.h"
#include "mulib-grpc.h"
#include "mulib-jsonrpc.h"
#include "mulib-rest.h"
#include "mulib-soap.h"
#include "mulib-sse-client.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_url(char url[128], const char *origin, const char *path)
{
  int n = snprintf(url, 128, "%s%s", origin, path);
  assert(n > 0 && n < 128);
}
static int record_event(void *user, const mulib_sse_event_t *event)
{
  int *count = (int *)user;
  assert(!strcmp(event->event, "sample") && !strcmp(event->id, "3"));
  assert(event->data_length == 7 && !memcmp(event->data, "one\ntwo", 7));
  ++*count;
  return 1;
}
int main(int argc, char **argv)
{
  mulib_muw3_client_t client = mulib_muw3_client_default();
  mulib_rest_response_t rest = {0};
  mulib_graphql_result_t graphql = {0};
  mulib_jsonrpc_reply_t rpc = {0};
  mulib_grpc_web_reply_t grpc = {0};
  mulib_muw3_response_t soap = {0}, sse = {0};
  mulib_sse_client_options_t options = {0};
  mulib_json_t *id;
  const mulib_json_t *data;
  char url[128], *doc;
  int count = 0;
  assert(argc == 2 && !strncmp(argv[1], "http://127.0.0.1:", 17));
  client.options.timeout_ms = 3000;
  client.options.max_redirects = 0;
  make_url(url, argv[1], "/rest");
  assert(mulib_rest_request(&client, "GET", url, NULL, 0, NULL, 1, &rest) == 1);
  assert(rest.http.message.status == 200 && mulib_json_boolean(mulib_json_get(rest.json,"ok")));
  mulib_rest_response_free(&rest);
  make_url(url, argv[1], "/graphql");
  assert(mulib_graphql_execute(&client,url,"query Probe { value }",NULL,"Probe",NULL,0,&graphql) == 1);
  data = mulib_json_get(graphql.response.json, "data");
  assert(!graphql.has_errors && mulib_json_numeric(mulib_json_get(data, "value")) == 42);
  mulib_graphql_result_free(&graphql);
  make_url(url, argv[1], "/rpc");
  id = mulib_json_string("test-id");
  assert(id && mulib_jsonrpc_call(&client,url,"demo.read",NULL,id,&rpc) == 1);
  assert(!rpc.remote_error && mulib_json_boolean(mulib_json_get(rpc.response.json,"result")));
  mulib_jsonrpc_reply_free(&rpc);
  mulib_json_destroy(id);
  make_url(url, argv[1], "/example.Probe/Read");
  assert(mulib_grpc_web_unary(&client,url,"\0a",2,256,&grpc) == 1);
  assert(grpc.grpc_status == 0 && grpc.message_length == 2 && !memcmp(grpc.message, "\0b", 2));
  mulib_grpc_web_reply_free(&grpc);
  make_url(url, argv[1], "/soap");
  doc = mulib_soap_envelope(1,NULL,"<p:Read xmlns:p='urn:probe'/>",1024);
  assert(doc && mulib_soap_call(&client,url,1,"urn:read",doc,&soap) == 1);
  assert(soap.message.status == 200 && soap.message.body_length);
  free(doc);
  mulib_muw3_response_free(&soap);
  make_url(url, argv[1], "/events");
  options.last_event_id = "2";
  assert(mulib_sse_client_get(&client,url,&options,record_event,&count,&sse) == 1);
  assert(sse.message.status == 200 && count == 1);
  mulib_muw3_response_free(&sse);
  return 0;
}
