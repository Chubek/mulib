/* A dependency-complete implementation TU, independent of missing legacy OCI
 * headers referenced by the repository-wide implementation fixture. */
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_WEBFRAMEWORK
#define MULIB_IMPL_VFS
#define MULIB_IMPL_RLE
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_ASSEMBLER
#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_LINKER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ELF
#define MULIB_IMPL_AMD64
#define MULIB_IMPL_ROW_RDBMS
#define MULIB_IMPL_COLUMN_RDBMS
#define MULIB_IMPL_GRAPHDB
#define MULIB_IMPL_OBJECTDB
#define MULIB_IMPL_EMBEDDINGSDB
#define MULIB_IMPL_DOCUMENTDB
#define MULIB_IMPL_DBSERIALIZER
#define MULIB_IMPL_BSON
#define MULIB_IMPL_MESSAGEPACK
#define MULIB_IMPL_PROTOBUF
#define MULIB_IMPL_DBSCHEMA
#define MULIB_IMPL_WEBIDL
#define MULIB_IMPL_MUIDL
#define MULIB_IMPL_WIT
#define MULIB_IMPL_CBOR
#define MULIB_IMPL_DBDESIALIZER
#define MULIB_IMPL_MULIB
#define MULIB_IMPL_ELFBUILDER
#define MULIB_IMPL_MACHOBUIDLER
#define MULIB_IMPL_PEBUILDER
#define MULIB_IMPL_COFFBUILDER
#define MULIB_IMPL_EVTLOOP
#define MULIB_IMPL_VHPST
#define MULIB_IMPL_STATIC_FILE
#define MULIB_IMPL_REWRITE_URL
#define MULIB_IMPL_COMPRESSION
#define MULIB_IMPL_UPSTREAM
#define MULIB_IMPL_LOAD_BALANCER
#define MULIB_IMPL_HEALTH_CHECKER
#define MULIB_IMPL_RATE_LIMIT
#define MULIB_IMPL_WAF
#define MULIB_IMPL_ACL
#define MULIB_IMPL_AUTH
#define MULIB_IMPL_SEURITY
#define MULIB_IMPL_OAUTH2
#define MULIB_IMPL_METRICS
#define MULIB_IMPL_HTTPSERVER_CONFIG
#define MULIB_IMPL_HTTP_REQUEST
#define MULIB_IMPL_HTTP_RESPONSE
#define MULIB_IMPL_MIME
#define MULIB_IMPL_WEB_CONNECTION
#define MULIB_IMPL_HTTP1
#define MULIB_IMPL_HTTP2
#define MULIB_IMPL_HTTP3
#define MULIB_IMPL_QUIC
#include "mulib-acl.h"
#include "mulib-amd64.h"
#include "mulib-coffbuilder.h"
#include "mulib-column-rdbms.h"
#include "mulib-compression.h"
#include "mulib-dbdesializer.h"
#include "mulib-elf.h"
#include "mulib-elfbuilder.h"
#include "mulib-embeddingsdb.h"
#include "mulib-evtloop.h"
#include "mulib-graphdb.h"
#include "mulib-health_checker.h"
#include "mulib-http2.h"
#include "mulib-http3.h"
#include "mulib-http_request.h"
#include "mulib-httpserver_config.h"
#include "mulib-load_balancer.h"
#include "mulib-machobuidler.h"
#include "mulib-metrics.h"
#include "mulib-muidl.h"
#include "mulib-mulib.h"
#include "mulib-oauth2.h"
#include "mulib-objectdb.h"
#include "mulib-pebuilder.h"
#include "mulib-protobuf.h"
#include "mulib-rate_limit.h"
#include "mulib-rewrite_url.h"
#include "mulib-row-rdbms.h"
#include "mulib-rpiter. static_file.h"
#include "mulib-vhpst.h"
#include "mulib-waf.h"
#include "mulib-web_conntection.h"
#include "mulib-webidl.h"
#include "mulib-wit.h"
#include <assert.h>

static mulib_json_t *
json (const char *s)
{
  mulib_json_t *v = mulib_json_parse (s, strlen (s), NULL);
  assert (v);
  return v;
}

static void
codecs (void)
{
  const char *source = "{\"text\":\"a\\u0000é\",\"number\":-1.25,\"array\":["
                       "true,false,null,{},[]]}";
  mulib_json_t *v = json (source);
  char *expected = mulib_json_stringify (v, 0);
  for (int f = MULIB_DBFORMAT_JSON; f <= MULIB_DBFORMAT_CBOR; ++f)
    {
      mulib_buffer_t b = { 0 };
      mulib_json_t *copy;
      char *text;
      assert (mulib_dbserializer_write ((mulib_dbformat)f, v, &b));
      copy = mulib_dbdeserializer_read ((mulib_dbformat)f, b.data, b.length);
      assert (copy);
      text = mulib_json_stringify (copy, 0);
      assert (text && !strcmp (text, expected));
      free (text);
      mulib_json_destroy (copy);
      for (size_t i = 0; i < b.length; ++i)
        {
          copy = mulib_dbdeserializer_read ((mulib_dbformat)f, b.data, i);
          assert (!copy);
        }
      assert (mulib_buffer_append (&b, "x", 1));
      assert (!mulib_dbdeserializer_read ((mulib_dbformat)f, b.data, b.length));
      mulib_buffer_free (&b);
    }
  assert (!mulib_cbor_decode ("\xbf\xff", 2)); /* indefinite map */
  assert (!mulib_cbor_decode ("\xa2\x61x\x01\x61x\x02", 7)); /* duplicate */
  assert (!mulib_messagepack_decode ("\x82\xa1x\x01\xa1x\x02", 7));
  assert (!mulib_messagepack_decode ("\xc4\x00", 2)); /* binary */
  assert (!mulib_cbor_decode ("\x1b\xff\xff\xff\xff\xff\xff\xff\xff", 9));
  assert (
      !mulib_messagepack_decode ("\xcf\xff\xff\xff\xff\xff\xff\xff\xff", 9));
  {
    mulib_json_t *x = mulib_cbor_decode ("\x83\x01\x21\xf5", 4);
    assert (x && mulib_json_size (x) == 3
            && mulib_json_numeric (mulib_json_at (x, 1)) == -2);
    mulib_json_destroy (x);
  }
  {
    mulib_json_t *x = mulib_messagepack_decode ("\x93\x01\xfe\xc3", 4);
    assert (x && mulib_json_numeric (mulib_json_at (x, 1)) == -2);
    mulib_json_destroy (x);
  }
  {
    mulib_json_t *x = json ("{\"a\\u0000b\":1}");
    mulib_buffer_t b = { 0 };
    assert (mulib_buffer_append (&b, "prefix", 6));
    assert (!mulib_bson_encode (x, &b) && b.length == 6);
    mulib_buffer_free (&b);
    mulib_json_destroy (x);
  }
  {
    unsigned char nested[70];
    memset (nested, 0x81, sizeof (nested));
    nested[69] = 0xf6;
    assert (!mulib_cbor_decode (nested, sizeof (nested)));
    memset (nested, 0x91, sizeof (nested));
    nested[69] = 0xc0;
    assert (!mulib_messagepack_decode (nested, sizeof (nested)));
  }
  {
    mulib_buffer_t b = { 0 };
    mulib_json_t *x = mulib_json_number (1.0);
    const unsigned char expected_number[]
        = { 0xfb, 0x3f, 0xf0, 0, 0, 0, 0, 0, 0 };
    assert (mulib_cbor_encode (x, &b) && b.length == 9
            && !memcmp (b.data, expected_number, 9));
    mulib_buffer_free (&b);
    mulib_json_destroy (x);
  }
  {
    mulib_buffer_t b = { 0 };
    mulib_json_t *x = json ("{\"hello\":\"world\"}");
    const unsigned char expected_bson[]
        = { 22, 0, 0, 0, 2,   'h', 'e', 'l', 'l', 'o', 0,
            6,  0, 0, 0, 'w', 'o', 'r', 'l', 'd', 0,   0 };
    assert (mulib_bson_encode (x, &b) && b.length == sizeof (expected_bson)
            && !memcmp (b.data, expected_bson, sizeof (expected_bson)));
    mulib_buffer_free (&b);
    mulib_json_destroy (x);
  }
  free (expected);
  mulib_json_destroy (v);
}

static int
neighbor (void *ctx, uint64_t label, uint64_t target)
{
  assert (label == 3 && target == 9);
  ++*(int *)ctx;
  return 1;
}

static void
storage (void)
{
  const mulib_dbschema_field_t fields[]
      = { { "name", MULIB_JSON_STRING, 1, 0 },
          { "score", MULIB_JSON_NUMBER, 0, 1 } };
  const mulib_dbschema_t schema = { "person", fields, 2 };
  mulib_row_rdbms_t rows = { 0 };
  mulib_column_rdbms_t cols = { NULL, 2, 0 };
  mulib_graphdb_t graph = { 0 };
  mulib_objectdb_t objects = { 0 };
  mulib_embeddingsdb_t embeddings = { 2, 0, NULL };
  mulib_json_t *v = json ("{\"name\":\"Ada\",\"score\":null}"), *copy;
  const double a[] = { 1, 2 }, b[] = { 3, 4 }, q[] = { 1.1, 2.1 };
  uint64_t id;
  double distance;
  const void *blob = NULL;
  size_t length = 0;
  int visits = 0;
  rows.schema = &schema;
  assert (mulib_dbschema_check (&schema, v));
  assert (!mulib_row_rdbms_put (&rows, 8, v, 2));
  copy = mulib_row_rdbms_get (&rows, 8);
  assert (copy);
  mulib_json_destroy (copy);
  assert (!mulib_row_rdbms_delete (&rows, 8, 3));
  assert (!mulib_row_rdbms_get (&rows, 8));
  assert (!mulib_row_rdbms_put (&rows, 8, v, 1));
  assert (!mulib_row_rdbms_get (&rows, 8)); /* stale */
  assert (mulib_json_set (v, "extra", mulib_json_bool (1)));
  assert (!mulib_dbschema_check (&schema, v));
  assert (mulib_row_rdbms_put (&rows, 8, v, 4) == MULIB_MEMTABLE_EINVAL);
  assert (mulib_column_rdbms_append (&cols, a, 2)
          && mulib_column_rdbms_append (&cols, b, 2));
  assert (mulib_column_rdbms_column (&cols, 0)[1] == 3
          && mulib_column_rdbms_column (&cols, 1)[0] == 2);
  assert (!mulib_column_rdbms_append (&cols, a, 1));
  assert (mulib_column_rdbms_append (&cols,
                                     mulib_column_rdbms_column (&cols, 0), 2));
  assert (!mulib_graphdb_edge (&graph, 1, 3, 9, 1, 0));
  assert (!mulib_graphdb_edge (&graph, 2, 3, 9, 1, 0));
  assert (mulib_graphdb_neighbors (&graph, 1, neighbor, &visits) == 1
          && visits == 1);
  assert (!mulib_graphdb_edge (&graph, 1, 3, 9, 2, 1));
  assert (!mulib_graphdb_neighbors (&graph, 1, neighbor, &visits));
  assert (!mulib_objectdb_put (&objects, "id", 2, 7, "a\0b", 3, 1));
  assert (!mulib_objectdb_get (&objects, "id", 2, 7, &blob, &length)
          && length == 3 && !memcmp (blob, "a\0b", 3));
  assert (mulib_objectdb_get (&objects, "id", 2, 8, &blob, &length) == 1);
  assert (
      !mulib_objectdb_put (&objects, "id", 2, 7, blob, length, 2)); /* alias */
  assert (!mulib_objectdb_delete (&objects, "id", 2, 3));
  assert (mulib_objectdb_get (&objects, "id", 2, 7, &blob, &length) == 1);
  assert (mulib_embeddingsdb_put (&embeddings, 9, a, 2)
          && mulib_embeddingsdb_put (&embeddings, 8, b, 2));
  assert (mulib_embeddingsdb_nearest (&embeddings, q, 2, &id, &distance)
          && id == 9 && distance < 0.15);
  assert (mulib_embeddingsdb_put (&embeddings, 7, a, 2));
  assert (mulib_embeddingsdb_nearest (&embeddings, a, 2, &id, &distance)
          && id == 7 && !distance);
  assert (mulib_embeddingsdb_delete (&embeddings, 7));
  for (int format = 0; format < 3; ++format)
    {
      mulib_buffer_t out = { 0 };
      assert (format == 0   ? mulib_muidl_generate (&schema, &out)
              : format == 1 ? mulib_webidl_generate (&schema, &out)
                            : mulib_wit_generate (&schema, &out));
      assert (out.length > 20);
      mulib_buffer_free (&out);
    }
  mulib_json_destroy (v);
  mulib_row_rdbms_free (&rows);
  mulib_column_rdbms_free (&cols);
  mulib_graphdb_free (&graph);
  mulib_objectdb_free (&objects);
  mulib_embeddingsdb_free (&embeddings);
}

static void
frames (void)
{
  mulib_buffer_t b = { 0 };
  uint64_t value = 99;
  size_t used = 77;
  const uint64_t values[]
      = { 0,     63,         64,         16383,
          16384, 1073741823, 1073741824, MULIB_QUIC_MAX_INTEGER };
  for (size_t i = 0; i < sizeof (values) / sizeof (values[0]); ++i)
    {
      b.length = 0;
      assert (mulib_quic_varint_write (&b, values[i]));
      for (size_t j = 0; j < b.length; ++j)
        {
          value = 99;
          used = 77;
          assert (!mulib_quic_varint_read (b.data, j, &value, &used)
                  && value == 99 && used == 77);
        }
      assert (mulib_quic_varint_read (b.data, b.length, &value, &used) == 1
              && value == values[i] && used == b.length);
    }
  {
    mulib_quic_stream_t f = { 4, 7, (const unsigned char *)"hi", 2, 1 }, copy;
    b.length = 0;
    assert (mulib_quic_stream_write (&b, &f));
    assert (mulib_quic_stream_read (b.data, b.length, &copy, &used) == 1
            && copy.offset == 7 && copy.fin && copy.length == 2);
    for (size_t i = 0; i < b.length; ++i)
      assert (!mulib_quic_stream_read (b.data, i, &copy, &used));
    f.offset = MULIB_QUIC_MAX_INTEGER;
    assert (!mulib_quic_stream_write (&b, &f));
  }
  {
    mulib_http3_frame_t f = { 0, (const unsigned char *)"hi", 2 }, copy;
    b.length = 0;
    assert (mulib_http3_frame_write (&b, &f));
    assert (mulib_http3_frame_read (b.data, b.length, 2, &copy, &used) == 1
            && used == b.length && copy.length == 2);
    assert (mulib_http3_frame_read (b.data, b.length, 1, &copy, &used) == -2);
  }
  {
    mulib_http2_frame_t f = { 0, 1, 1, (const unsigned char *)"hi", 2 }, copy;
    b.length = 0;
    assert (mulib_http2_frame_write (&b, &f));
    assert (mulib_http2_frame_read (b.data, b.length, 16384, &copy, &used) == 1
            && copy.stream == 1 && used == 11);
    for (size_t i = 0; i < b.length; ++i)
      assert (!mulib_http2_frame_read (b.data, i, 16384, &copy, &used));
    f.stream = 0;
    assert (!mulib_http2_frame_write (&b, &f));
    f.type = 6;
    f.length = 7;
    assert (!mulib_http2_frame_write (&b, &f));
  }
  for (unsigned wire = 0; wire <= 5; ++wire)
    if (wire == 0 || wire == 1 || wire == 2 || wire == 5)
      {
        mulib_protobuf_field_t f
            = { 300, wire, 123, (const unsigned char *)"a\0b", 3 },
            copy;
        b.length = 0;
        assert (mulib_protobuf_write (&b, &f));
        assert (mulib_protobuf_read (b.data, b.length, &copy, &used) == 1
                && copy.number == 300 && copy.wire == wire && used == b.length);
        for (size_t i = 0; i < b.length; ++i)
          assert (!mulib_protobuf_read (b.data, i, &copy, &used));
      }
  assert (mulib_protobuf_unzigzag (mulib_protobuf_zigzag (INT64_MIN))
          == INT64_MIN);
  assert (mulib_protobuf_unzigzag (mulib_protobuf_zigzag (INT64_MAX))
          == INT64_MAX);
  {
    const unsigned char known[] = { 8, 150, 1 };
    mulib_protobuf_field_t f;
    assert (mulib_protobuf_read (known, 3, &f, &used) == 1 && f.number == 1
            && f.value == 150);
  }
  mulib_buffer_free (&b);
}

static int
verifier (void *ctx, const char *token, const char *scope)
{
  ++*(int *)ctx;
  return !strcmp (token, "abc") && !strcmp (scope, "read");
}
static void
policies (void)
{
  char path[100] = "unchanged", basic[100];
  size_t n = 99, cursor = 0;
  mulib_rate_limit_t rate;
  mulib_metrics_t metrics = { 0 };
  int called = 0;
  mulib_acl_rule_t rules[] = { { 0x0a000000, 8, 1 }, { 0, 0, 0 } };
  mulib_upstream_t up[] = { { "a", 80, 0, 1, 1 }, { "b", 80, 0, 0, 1 } };
  mulib_health_checker_t health = { 0, 0, 2, 2 };
  mulib_vhpst_t hosts[] = { { "example.org", NULL } };
  assert (mulib_security_path ("/a%20b", path, sizeof (path))
          && !strcmp (path, "/a b"));
  assert (!mulib_security_path ("/%2e%2e/x", path, sizeof (path))
          && !strcmp (path, "/a b"));
  assert (!mulib_security_path ("/a%2fb", path, sizeof (path))
          && !mulib_security_path ("//a", path, sizeof (path)));
  assert (!mulib_security_path ("/%00", path, sizeof (path))
          && !mulib_security_path ("/%252e", path, sizeof (path)));
  assert (mulib_acl_check (rules, 2, 0x0a010203, 0)
          && !mulib_acl_check (rules, 2, 0x7f000001, 1));
  rules[1].prefix = 33;
  assert (!mulib_acl_check (rules, 2, 0x0a010203, 1));
  assert (mulib_rate_limit_init (&rate, 2, 3, 0));
  assert (mulib_rate_limit_take (&rate, 3, 0)
          && !mulib_rate_limit_take (&rate, 1, 0));
  assert (mulib_rate_limit_take (&rate, 2, 1)
          && !mulib_rate_limit_take (&rate, 1, 0));
  assert (!mulib_rate_limit_take (&rate, 1, NAN));
  assert (mulib_load_balancer_select (up, 2, &cursor, 0) == 0
          && mulib_upstream_acquire (up));
  assert (mulib_load_balancer_select (up, 2, &cursor, 1) == 1);
  assert (mulib_health_checker_record (&health, up + 1, 0) && up[1].healthy);
  assert (mulib_health_checker_record (&health, up + 1, 0) && !up[1].healthy);
  assert (mulib_load_balancer_select (up, 2, &cursor, 0) == SIZE_MAX);
  assert (mulib_upstream_release (up) && !mulib_upstream_release (up));
  assert (mulib_health_checker_record (&health, up + 1, 1) && !up[1].healthy);
  assert (mulib_health_checker_record (&health, up + 1, 1) && up[1].healthy);
  assert (mulib_auth_basic ("Basic YTpi", basic, sizeof (basic), &n) && n == 3
          && !strcmp (basic, "a:b"));
  assert (!mulib_auth_basic ("Basic YTpi", basic, 3, &n) && n == 3);
  assert (mulib_auth_bearer ("bearer abc")
          && !mulib_auth_bearer ("Bearer a b"));
  assert (!mulib_oauth2_authorize ("Bearer abc", "read", NULL, &called));
  assert (mulib_oauth2_authorize ("Bearer abc", "read", verifier, &called)
          && called == 1);
  assert (mulib_oauth2_state ("nonce", "nonce") && !mulib_oauth2_state ("", "")
          && !mulib_oauth2_state ("abc", "abd"));
  metrics.requests = UINT64_MAX;
  assert (mulib_metrics_record (&metrics, 200, 1, 2, 3)
          && metrics.requests == UINT64_MAX && metrics.status_class[2] == 1);
  assert (!strcmp (mulib_mime_type ("a.PNG"), "image/png"));
  assert (mulib_vhpst_find (hosts, 1, "EXAMPLE.ORG") == 0
          && mulib_vhpst_find (hosts, 1, "example.org:80") == SIZE_MAX);
  {
    char *out = NULL;
    assert (mulib_rewrite_url ("/old/file?q=1", "/old", "/new", &out) == 1
            && !strcmp (out, "/new/file?q=1"));
    free (out);
    assert (mulib_rewrite_url ("/older", "/old", "/new", &out) == 0);
    assert (mulib_rewrite_url ("/../a", "/", "/new", &out) == -1);
  }
  {
    mulib_buffer_t compressed = { 0 }, decoded = { 0 };
    assert (mulib_compression_encode (MULIB_COMPRESSION_RLE, "aaaaabb", 7,
                                      &compressed));
    assert (!mulib_compression_decode (MULIB_COMPRESSION_RLE, compressed.data,
                                       compressed.length, 6, &decoded)
            && !decoded.length);
    assert (mulib_compression_decode (MULIB_COMPRESSION_RLE, compressed.data,
                                      compressed.length, 7, &decoded)
            && decoded.length == 7 && !memcmp (decoded.data, "aaaaabb", 7));
    mulib_buffer_free (&compressed);
    mulib_buffer_free (&decoded);
  }
}

static void
http_helpers (void)
{
  mulib_http_header_t host = { (char *)"Host", (char *)"example.org" };
  mulib_http_message_t request = { 0 }, reply = { 0 }, parsed = { 0 };
  mulib_web_connection_t connection = { 0 };
  unsigned char *wire;
  size_t length;
  mulib_httpserver_config_t config = mulib_httpserver_config_default ();
  const char *methods[] = { "GET" };
  mulib_waf_t waf = { 1000, 100, methods, 1 };
  mulib_vfs vfs;
  mulib_vfs_mem mem;
  mulib_vfs_ops ops;
  assert (mulib_httpserver_config_validate (&config));
  connection.limit = config.max_buffer_bytes;
  config.limits.max_body_bytes = 0;
  assert (!mulib_httpserver_config_validate (&config));
  assert (mulib_http_request_create (&request, "GET", "/a", &host, 1, NULL, 0)
          == MULIB_HTTP_OK);
  assert (mulib_waf_check (&waf, &request));
  assert (mulib_http_serialize (&request, NULL, &wire, &length)
          == MULIB_HTTP_OK);
  assert (mulib_web_connection_feed (&connection, wire, length / 2, 0)
          == MULIB_HTTP_OK);
  assert (mulib_web_connection_next (&connection, &parsed) == MULIB_HTTP_MORE);
  assert (mulib_web_connection_feed (&connection, wire + length / 2,
                                     length - length / 2, 0)
          == MULIB_HTTP_OK);
  assert (mulib_web_connection_feed (&connection, wire, length, 1)
          == MULIB_HTTP_OK);
  assert (mulib_web_connection_next (&connection, &parsed) == MULIB_HTTP_OK);
  mulib_http_message_free (&parsed);
  assert (mulib_web_connection_next (&connection, &parsed) == MULIB_HTTP_OK);
  mulib_http_message_free (&parsed);
  assert (!connection.pending.length
          && mulib_web_connection_feed (&connection, "", 0, 0)
                 == MULIB_HTTP_INVALID);
  free (wire);
  mulib_http_message_free (&request);
  mulib_web_connection_free (&connection);
  assert (mulib_vfs_init (&vfs, 1) == 0
          && mulib_vfs_mem_init (&mem, 10, 1000) == 0);
  ops = mulib_vfs_mem_ops ();
  assert (mulib_vfs_mount (&vfs, "/", &ops, &mem) == 0
          && mulib_vfs_mem_put (&mem, "/a.txt", "abc", 3) == 0);
  assert (mulib_static_file_reply (&vfs, "/a.txt", 2, &reply)
          == MULIB_HTTP_LIMIT);
  assert (mulib_static_file_reply (&vfs, "/a.txt", 3, &reply) == MULIB_HTTP_OK
          && reply.body_length == 3);
  mulib_http_message_free (&reply);
  mulib_vfs_free (&vfs);
  mulib_vfs_mem_free (&mem);
}

static void
increment (void *ctx)
{
  ++*(int *)ctx;
}
typedef struct task_context
{
  mulib_evtloop_t *loop;
  int count, order;
  uint64_t cancel;
} task_context;
static void
task_again (void *ctx)
{
  task_context *c = (task_context *)ctx;
  ++c->count;
  assert (mulib_evtloop_run (c->loop, c->loop->now, 1) == SIZE_MAX);
  if (c->cancel)
    {
      assert (mulib_evtloop_cancel (c->loop, c->cancel));
      c->cancel = 0;
    }
  assert (mulib_evtloop_post (c->loop, c->loop->now, task_again, c));
}
static void
task_first (void *ctx)
{
  task_context *c = (task_context *)ctx;
  c->order = c->order * 10 + 1;
}
static void
task_second (void *ctx)
{
  task_context *c = (task_context *)ctx;
  c->order = c->order * 10 + 2;
}
static void
builders (void)
{
  const unsigned char code[] = { 0xc3 };
  unsigned char storage[20];
  mulib_amd64_t amd64 = { 0 };
  mulib_emitter_t emitter;
  mulib_buffer_t b = { 0 };
  mulib_elfbuilder_t elf = { 0 };
  mulib_elf_t view;
  mulib_elf_section_t section;
  assert (mulib_amd64_init (&amd64) && !mulib_amd64_init (&amd64));
  assert (!mulib_emitter_init (&emitter, storage, sizeof (storage),
                               MULIB_EMITTER_LITTLE));
  assert (!mulib_amd64_emit (&amd64, &emitter, NULL, "ret", NULL, 0, NULL)
          && emitter.size == 1 && storage[0] == 0xc3);
  mulib_amd64_free (&amd64);
  assert (mulib_elfbuilder_add (&elf, ".text", code, 1, 16, 6));
  assert (mulib_elfbuilder_finish (&elf, 62, &b)
          && !mulib_elf_open (&view, b.data, b.length));
  assert (!mulib_elf_section (&view, 1, &section) && section.size == 1
          && b.data[section.offset] == 0xc3);
  mulib_elfbuilder_free (&elf);
  mulib_buffer_free (&b);
  assert (mulib_coffbuilder_text (&b, code, 1) && b.length == 61
          && b.data[0] == 0x64 && b.data[60] == 0xc3);
  mulib_buffer_free (&b);
  assert (mulib_machobuilder_text (&b, code, 1) && b.length == 193
          && b.data[0] == 0xcf && b.data[192] == 0xc3);
  mulib_buffer_free (&b);
  assert (mulib_pebuilder_text (&b, code, 1, 0) && b.length == 1024
          && !memcmp (b.data, "MZ", 2) && b.data[512] == 0xc3);
  assert (!mulib_pebuilder_text (&b, code, 1, 1) && b.length == 1024);
  mulib_buffer_free (&b);
  {
    mulib_evtloop_t loop = { 0 };
    int n = 0;
    uint64_t cancelled;
    loop.limit = 2;
    cancelled = mulib_evtloop_post (&loop, 1, increment, &n);
    assert (cancelled);
    assert (mulib_evtloop_post (&loop, 2, increment, &n));
    assert (!mulib_evtloop_post (&loop, 2, increment, &n));
    assert (mulib_evtloop_cancel (&loop, cancelled));
    assert (!mulib_evtloop_run (&loop, 1, 100) && !n);
    assert (mulib_evtloop_run (&loop, 2, 1) == 1 && n == 1);
    assert (mulib_evtloop_run (&loop, 1, 1) == SIZE_MAX);
    mulib_evtloop_free (&loop);
  }
  {
    mulib_evtloop_t loop = { 0 };
    task_context context = { &loop, 0, 0, 0 };
    loop.limit = 4;
    assert (mulib_evtloop_post (&loop, 2, task_first, &context));
    assert (mulib_evtloop_post (&loop, 2, task_second, &context));
    assert (mulib_evtloop_run (&loop, 2, 2) == 2 && context.order == 12);
    assert (mulib_evtloop_post (&loop, 2, task_again, &context));
    context.cancel = mulib_evtloop_post (&loop, 3, task_first, &context);
    assert (context.cancel);
    assert (mulib_evtloop_run (&loop, 3, 5) == 5 && context.count == 5);
    assert (loop.count == 1); /* immediately re-posted work is budget-bounded */
    mulib_evtloop_free (&loop);
  }
  assert (!strcmp (mulib_version (), MULIB_VERSION_STRING));
}

static void
mutations (void)
{
  uint32_t state = 0x12345678;
  mulib_buffer_t seeds[3] = { { 0 } };
  mulib_json_t *seed
      = json ("{\"items\":[1,-2,{},[],null],\"text\":\"hello\"}");
  for (unsigned i = 0; i < 3; ++i)
    assert (
        mulib_dbserializer_write ((mulib_dbformat)(i + 1), seed, seeds + i));
  mulib_json_destroy (seed);
  for (unsigned iteration = 0; iteration < 10000; ++iteration)
    {
      unsigned char bytes[64];
      size_t n = iteration % sizeof (bytes), used;
      mulib_json_t *v;
      mulib_protobuf_field_t pb;
      mulib_http2_frame_t h2;
      mulib_http3_frame_t h3;
      mulib_quic_stream_t stream;
      for (size_t i = 0; i < n; ++i)
        {
          state = state * 1664525U + 1013904223U;
          bytes[i] = (unsigned char)(state >> 24);
        }
      v = mulib_cbor_decode (bytes, n);
      mulib_json_destroy (v);
      v = mulib_messagepack_decode (bytes, n);
      mulib_json_destroy (v);
      v = mulib_bson_decode (bytes, n);
      mulib_json_destroy (v);
      (void)mulib_protobuf_read (bytes, n, &pb, &used);
      (void)mulib_http2_frame_read (bytes, n, 100, &h2, &used);
      (void)mulib_http3_frame_read (bytes, n, 100, &h3, &used);
      (void)mulib_quic_stream_read (bytes, n, &stream, &used);
      for (unsigned codec = 0; codec < 3; ++codec)
        {
          unsigned char changed[256];
          size_t length = seeds[codec].length;
          assert (length && length < sizeof (changed));
          memcpy (changed, seeds[codec].data, length);
          state = state * 1664525U + 1013904223U;
          changed[state % length] ^= (unsigned char)(state >> 24);
          if (iteration & 1)
            length = (size_t)(state % length);
          v = mulib_dbdeserializer_read ((mulib_dbformat)(codec + 1), changed,
                                         length);
          mulib_json_destroy (v);
        }
    }
  for (unsigned i = 0; i < 3; ++i)
    mulib_buffer_free (seeds + i);
}

static void
emit_fixtures (void)
{
  mulib_buffer_t b = { 0 };
  mulib_elfbuilder_t elf = { 0 };
  const unsigned char code[] = { 0x31, 0xc0, 0xc3 };
  const char *names[]
      = { "text.elf", "text.obj", "text.macho", "text.exe", "value.msgpack" };
  for (unsigned i = 0; i < 5; ++i)
    {
      FILE *f;
      b.length = 0;
      if (!i)
        {
          assert (
              mulib_elfbuilder_add (&elf, ".text", code, sizeof (code), 16, 6));
          assert (mulib_elfbuilder_finish (&elf, 62, &b));
        }
      else if (i == 1)
        assert (mulib_coffbuilder_text (&b, code, sizeof (code)));
      else if (i == 2)
        assert (mulib_machobuilder_text (&b, code, sizeof (code)));
      else if (i == 3)
        assert (mulib_pebuilder_text (&b, code, sizeof (code), 0));
      else
        {
          mulib_json_t *v
              = json ("{\"n\":1.5,\"s\":\"abc\",\"a\":[true,null]}");
          assert (mulib_messagepack_encode (v, &b));
          mulib_json_destroy (v);
        }
      f = fopen (names[i], "wb");
      assert (f);
      assert (fwrite (b.data, 1, b.length, f) == b.length);
      assert (!fclose (f));
    }
  mulib_elfbuilder_free (&elf);
  mulib_buffer_free (&b);
}

int
main (int argc, char **argv)
{
  codecs ();
  storage ();
  frames ();
  policies ();
  http_helpers ();
  builders ();
  mutations ();
  if (argc == 2 && !strcmp (argv[1], "--emit"))
    emit_fixtures ();
  puts ("storage/server extensions: OK");
  return 0;
}
