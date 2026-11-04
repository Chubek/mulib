#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_HTTP1
#define MULIB_IMPL_HTTP2
#define MULIB_IMPL_HTTP3
#define MULIB_IMPL_QUIC
#define MULIB_IMPL_HTTP_REQUEST
#define MULIB_IMPL_HTTP_RESPONSE
#define MULIB_IMPL_HTTPSERVER_CONFIG
#define MULIB_IMPL_WEBFRAMEWORK
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_TSL
#define MULIB_IMPL_HTTPS
#include "mulib-http1.h"
#include "mulib-http2.h"
#include "mulib-http3.h"
#include "mulib-http_request.h"
#include "mulib-http_response.h"
#include "mulib-httpserver_config.h"
#include "mulib-https.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
http1 (void)
{
  static const char request[] = "POST / HTTP/1.1\r\nHost: example.test\r\nContent-Length: 3\r\n\r\na\0b";
  static const char *const malformed[] = {
    "POST / HTTP/1.1\r\nHost:x\r\nContent-Length:1\r\ncontent-length:1\r\n\r\nx",
    "POST / HTTP/1.1\r\nHost:x\r\nContent-Length:1\r\nTransfer-Encoding:chunked\r\n\r\nx",
    "POST / HTTP/1.1\r\nHost:x\r\nTransfer-Encoding:chunked\r\n\r\n1\r\nx\r\n0\r\nX:y\r\n\r\n",
    "GET / HTTP/1.1\r\nHost:x\r\n bad:fold\r\n\r\n",
    "GET / HTTP/1.1\r\nHost:x\r\nX :y\r\n\r\n",
    "GET / HTTP/1.1\r\nHost:x\n\r\n",
    "GET / HTTP/1.1\r\nHost:x:99999999999999999999999\r\n\r\n"
  };
  mulib_http_message_t m = {0}, sentinel = {0};
  mulib_http_options_t o = {0,0,0,0};
  size_t used, n = 0;
  unsigned char *wire = NULL;
  sentinel.status = 599;
  for (size_t i = 0; i < sizeof (request) - 1; ++i) {
    m = sentinel; used = 99;
    assert (mulib_http1_parse (request,i,0,NULL,&m,&used) == MULIB_HTTP_MORE);
    assert (!used && !memcmp (&m,&sentinel,sizeof (m)));
    o.flags = MULIB_HTTP_EOF;
    assert (mulib_http1_parse (request,i,0,&o,&m,&used) == MULIB_HTTP_INVALID);
    assert (!used && !memcmp (&m,&sentinel,sizeof (m)));
  }
  assert (mulib_http1_parse (request,sizeof (request)-1,0,NULL,&m,&used) == MULIB_HTTP_OK);
  assert (used == sizeof (request)-1 && m.body_length == 3 && !memcmp (m.body,"a\0b",3));
  assert (mulib_http1_serialize (&m,NULL,&wire,&n) == MULIB_HTTP_OK);
  assert (n == sizeof (request)-1 && !memcmp (wire,request,n));
  free (wire); wire = NULL; mulib_http_message_free (&m);
  for (size_t i = 0; i < sizeof (malformed)/sizeof (*malformed); ++i) {
    m = sentinel; used = 99;
    assert (mulib_http_parse (malformed[i],strlen (malformed[i]),0,&o,&m,&used) == MULIB_HTTP_INVALID);
    assert (!used && !memcmp (&m,&sentinel,sizeof (m)));
  }
  {
    const char *replies[] = {
      "HTTP/1.1 205 Reset Content\r\nContent-Length: 0\r\n\r\n",
      "HTTP/1.1 205 Reset Content\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
      "HTTP/1.1 205 Reset Content\r\n\r\n",
      "HTTP/1.1 205 Reset Content\r\nContent-Length: 1\r\n\r\nx",
      "HTTP/1.1 205 Reset Content\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx\r\n0\r\n\r\n",
      "HTTP/1.1 205 Reset Content\r\n\r\nx"
    };
    for (size_t i = 0; i < sizeof (replies)/sizeof (*replies); ++i) {
      int rc = mulib_http_parse (replies[i],strlen (replies[i]),1,&o,&m,&used);
      assert (rc == (i < 3 ? MULIB_HTTP_OK : MULIB_HTTP_INVALID));
      if (rc == MULIB_HTTP_OK) { assert (!m.body_length); mulib_http_message_free (&m); }
    }
  }
  {
    mulib_http_header_t h[] = {{"Host","x"},{"X","0123456789"}};
    unsigned char mark = 42, *output = &mark;
    m = (mulib_http_message_t){0}; m.method = "GET"; m.target = "/";
    m.headers = h; m.header_count = 2;
    o = (mulib_http_options_t){0,0,0,0};
    assert (mulib_http_serialize (&m,&o,&wire,&n) == MULIB_HTTP_OK);
    o.max_header_bytes = n;
    assert (mulib_http_serialize (&m,&o,&output,&used) == MULIB_HTTP_OK && used == n);
    free (output); output = &mark; used = 99; --o.max_header_bytes;
    assert (mulib_http_serialize (&m,&o,&output,&used) == MULIB_HTTP_LIMIT);
    assert (output == &mark && used == 99);
    o.max_header_bytes = 0; o.max_headers = 2; /* Generated CL is the third field. */
    assert (mulib_http_serialize (&m,&o,&output,&used) == MULIB_HTTP_LIMIT);
    o.max_headers = 0;
    for (unsigned flags = 2; flags <= 6; flags += 2) {
      o.flags = flags;
      assert (mulib_http_parse (request,sizeof (request)-1,0,&o,&sentinel,&used) == MULIB_HTTP_INVALID);
      assert (mulib_http_serialize (&m,&o,&output,&used) == MULIB_HTTP_INVALID);
    }
    m.is_response = 1; m.status = 200; m.reason = "OK";
    assert (mulib_http_serialize (&m,&o,&output,&used) == MULIB_HTTP_INVALID);
    free (wire); wire = NULL;
  }
  {
    mulib_http_header_t h[] = {{"Host","x"},{"transfer-encoding"," ChUnKeD "}};
    m = (mulib_http_message_t){0};
    assert (mulib_http_request_create (&m,"POST","/",h,2,"a\0b",3) == MULIB_HTTP_OK);
    assert (m.chunked && m.body_length == 3 && !memcmp (m.body,"a\0b",3));
    assert (m.headers != h && m.headers[0].name != h[0].name);
    mulib_http_message_free (&m);
    h[1].value = "gzip, chunked";
    assert (mulib_http_request_create (&m,"POST","/",h,2,"abc",3) == MULIB_HTTP_INVALID);
    assert (!m.body && !m.headers);
    assert (mulib_http_response_create (&m,205,NULL,NULL,0) == MULIB_HTTP_OK);
    mulib_http_message_free (&m);
    assert (mulib_http_response_create (&m,205,NULL,"x",1) == MULIB_HTTP_INVALID);
    assert (mulib_http_response_create (&m,200,"text/plain\r\nX: bad",NULL,0) == MULIB_HTTP_INVALID);
  }
  {
    mulib_httpserver_config_t c = mulib_httpserver_config_default (), original = c;
    assert (mulib_httpserver_config_validate (&c));
    c.max_connections = SIZE_MAX; assert (!mulib_httpserver_config_validate (&c)); c = original;
    c.limits.max_headers = SIZE_MAX; assert (!mulib_httpserver_config_validate (&c)); c = original;
    c.limits.max_body_bytes = SIZE_MAX; assert (!mulib_httpserver_config_validate (&c)); c = original;
    c.max_buffer_bytes = 0; assert (!mulib_httpserver_config_validate (&c)); c = original;
    c.limits.flags = MULIB_HTTP_EOF; assert (!mulib_httpserver_config_validate (&c));
  }
}

static void
h2_case (unsigned type, unsigned flags, uint32_t stream, const unsigned char *p, size_t n, int valid)
{
  unsigned char wire[512];
  mulib_http2_frame_t f, before, source;
  mulib_buffer_t output = {0};
  size_t used = 99;
  assert (n + 9 <= sizeof (wire));
  wire[0] = 0; wire[1] = (unsigned char)(n >> 8); wire[2] = (unsigned char)n;
  wire[3] = (unsigned char)type; wire[4] = (unsigned char)flags;
  for (unsigned i = 0; i < 4; ++i) wire[5+i] = (unsigned char)(stream >> (24-8*i));
  if (n) memcpy (wire+9,p,n);
  memset (&before,0xa5,sizeof (before)); f = before;
  assert (mulib_http2_frame_read (wire,n+9,512,&f,&used) == (valid ? 1 : -1));
  if (valid) {
    assert (used == n+9 && f.stream == (stream & UINT32_C (0x7fffffff)));
    for (size_t i = 0; i < n+9; ++i) {
      used = 99; f = before;
      assert (mulib_http2_frame_read (wire,i,512,&f,&used) == 0);
      assert (used == 99 && !memcmp (&f,&before,sizeof (f)));
    }
  } else assert (used == 99 && !memcmp (&f,&before,sizeof (f)));
  source.type = (unsigned char)type; source.flags = (unsigned char)flags;
  source.stream = stream & UINT32_C (0x7fffffff); source.data = p; source.length = n;
  assert (mulib_buffer_append (&output,"prefix",6));
  assert (mulib_http2_frame_write (&output,&source) == valid);
  assert (!memcmp (output.data,"prefix",6));
  if (!valid) assert (output.length == 6);
  mulib_buffer_free (&output);
}
static void
http2 (void)
{
  const unsigned char priority_self[] = {0x80,0,0,1,255};
  const unsigned char priority_root[] = {0,0,0,0,0};
  const unsigned char padded_priority[] = {2,0x80,0,0,1,255,0,0};
  const unsigned char padding_only[] = {2,0,0};
  const unsigned char push_zero[] = {0x80,0,0,0};
  const unsigned char push_id[] = {0x80,0,0,2};
  const unsigned char settings_good[] = {0,2,0,0,0,1, 0,2,0,0,0,0, 0,5,0,0,64,0, 0xff,0xff,0xff,0xff,0xff,0xff};
  unsigned char setting[] = {0,2,0,0,0,2};
  h2_case (2,0,1,priority_self,5,0);
  h2_case (2,0,1,priority_root,5,1);
  h2_case (1,32,1,priority_self,5,0);
  h2_case (1,40,1,padded_priority,sizeof (padded_priority),0);
  h2_case (1,40,2,padded_priority,sizeof (padded_priority),1);
  h2_case (1,40,2,padded_priority,6,0);
  h2_case (0,8,1,padding_only,3,1);
  h2_case (0,8,1,padding_only,2,0);
  h2_case (5,0,1,push_zero,4,0);
  h2_case (5,0,1,push_id,4,1);
  h2_case (4,0,0,settings_good,sizeof (settings_good),1);
  h2_case (4,1,0,settings_good,sizeof (settings_good),0);
  h2_case (4,0,0,setting,6,0);
  setting[1] = 8; h2_case (4,0,0,setting,6,0);
  setting[1] = 9; h2_case (4,0,0,setting,6,0);
  setting[1] = 4; setting[2] = 0x80; setting[5] = 0; h2_case (4,0,0,setting,6,0);
  setting[2] = 0x7f; setting[3] = setting[4] = setting[5] = 0xff; h2_case (4,0,0,setting,6,1);
  setting[1] = 5; setting[2] = setting[3] = setting[4] = 0; setting[5] = 1; h2_case (4,0,0,setting,6,0);
  h2_case (8,0,0,push_zero,4,0);
  h2_case (8,0,0,push_id,4,1);
  h2_case (4,1,0,NULL,0,1);
  h2_case (0xee,0xff,UINT32_C (0x80000001),padding_only,3,1);
  {
    const unsigned char header[] = {0,0,8,6,0,0,0,0,0};
    mulib_http2_frame_t f = {0}; size_t used = 9;
    assert (mulib_http2_frame_read (header,9,7,&f,&used) == -2 && used == 9);
  }
  {
    mulib_buffer_t b = {0}; mulib_http2_frame_t f = {0,0,1,NULL,0};
    assert (mulib_buffer_append (&b,"payload",7)); f.data = b.data; f.length = b.length;
    assert (mulib_http2_frame_write (&b,&f) && b.length == 23);
    assert (!memcmp (b.data+16,"payload",7)); mulib_buffer_free (&b);
  }
}

static void
h3_case (uint64_t type, const unsigned char *p, size_t n, int valid)
{
  mulib_buffer_t raw = {0}, output = {0};
  mulib_http3_frame_t f, before, source = {type,p,n};
  size_t used = 99;
  assert (mulib_quic_varint_write (&raw,type) && mulib_quic_varint_write (&raw,n));
  assert (mulib_buffer_append (&raw,p,n));
  memset (&before,0xa5,sizeof (before)); f = before;
  assert (mulib_http3_frame_read (raw.data,raw.length,1024,&f,&used) == (valid ? 1 : -1));
  if (valid) {
    assert (used == raw.length && f.type == type && f.length == n);
    for (size_t i = 0; i < raw.length; ++i) {
      used = 99; f = before;
      assert (mulib_http3_frame_read (raw.data,i,1024,&f,&used) == 0);
      assert (used == 99 && !memcmp (&f,&before,sizeof (f)));
    }
  } else assert (used == 99 && !memcmp (&f,&before,sizeof (f)));
  assert (mulib_buffer_append (&output,"prefix",6));
  assert (mulib_http3_frame_write (&output,&source) == valid);
  assert (!memcmp (output.data,"prefix",6));
  if (!valid) assert (output.length == 6);
  mulib_buffer_free (&raw); mulib_buffer_free (&output);
}
static void
http3 (void)
{
  const unsigned char zero[] = {0};
  const unsigned char nonminimal[] = {0xc0,0,0,0,0,0,0,1};
  const unsigned char incomplete[] = {0x40};
  const unsigned char trailing[] = {1,0};
  const unsigned char settings[] = {1,0,6,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,8,1,0x33,1};
  const unsigned char duplicate[] = {1,0,0x40,1,1};
  const unsigned char duplicate_unknown[] = {0x40,0xff,0,0x40,0xff,1};
  const unsigned char missing_value[] = {1};
  unsigned char boolean[] = {8,2}, reserved[] = {2,0};
  h3_case (4,NULL,0,1);
  h3_case (4,settings,sizeof (settings),1);
  h3_case (4,duplicate,sizeof (duplicate),0);
  h3_case (4,duplicate_unknown,sizeof (duplicate_unknown),0);
  h3_case (4,missing_value,1,0);
  h3_case (4,boolean,2,0); boolean[0] = 0x33; h3_case (4,boolean,2,0);
  for (unsigned i = 2; i <= 5; ++i) { reserved[0] = (unsigned char)i; h3_case (4,reserved,2,0); }
  for (unsigned i = 0; i < 3; ++i) {
    unsigned type = i == 0 ? 3 : i == 1 ? 7 : 13;
    h3_case (type,zero,1,1); h3_case (type,nonminimal,8,1);
    h3_case (type,NULL,0,0); h3_case (type,incomplete,1,0); h3_case (type,trailing,2,0);
  }
  h3_case (5,NULL,0,0); h3_case (5,trailing,2,1);
  h3_case (2,NULL,0,0); h3_case (6,NULL,0,0); h3_case (8,NULL,0,0); h3_case (9,NULL,0,0);
  h3_case (0x21,incomplete,1,1); /* Unknown/grease payloads remain opaque. */
  {
    mulib_buffer_t settings_buffer = {0};
    for (unsigned i = 64; i < 128; ++i) {
      assert (mulib_quic_varint_write (&settings_buffer,i));
      assert (mulib_quic_varint_write (&settings_buffer,i));
    }
    h3_case (4,settings_buffer.data,settings_buffer.length,1);
    assert (mulib_quic_varint_write (&settings_buffer,64));
    assert (mulib_quic_varint_write (&settings_buffer,0));
    h3_case (4,settings_buffer.data,settings_buffer.length,0);
    mulib_buffer_free (&settings_buffer);
  }
  {
    const unsigned char wire[] = {0x40,0,0x40,1,'x','y'}; /* Nonminimal type and length. */
    mulib_http3_frame_t f = {0}; size_t used = 99;
    assert (mulib_http3_frame_read (wire,sizeof (wire),1,&f,&used) == 1 && used == 5 && f.data[0] == 'x');
    used = 99;
    assert (mulib_http3_frame_read (wire,4,0,&f,&used) == -2 && used == 99);
  }
}

typedef struct tls_fixture {
  const char *reply;
  size_t offset;
  int connected, closed, verified, mode;
  mulib_buffer_t sent;
} tls_fixture;
static int tls_handshake (void *p, int *verified)
{ tls_fixture *f = p; *verified = f->verified; return f->mode == 2 ? MULIB_TSL_AGAIN : 0; }
static int tls_write (void *p, const void *data, size_t n, size_t *count)
{
  tls_fixture *f = p;
  if (f->mode == 3) { *count = n + 1; return 0; }
  if (n > 3) n = 3;
  if (!mulib_buffer_append (&f->sent,data,n)) return MULIB_TSL_EBACKEND;
  *count = n; return 0;
}
static int tls_read (void *p, void *data, size_t n, size_t *count)
{
  tls_fixture *f = p;
  size_t left = strlen (f->reply) - f->offset;
  if (!left) return f->mode == 4 ? MULIB_TSL_EBACKEND : MULIB_TSL_EOF;
  if (n > left) n = left;
  if (n > 7) n = 7;
  memcpy (data,f->reply+f->offset,n); f->offset += n; *count = n;
  return 0;
}
static void tls_close (void *p) { ++((tls_fixture *)p)->closed; }
static int tls_connect (void *p, const mulib_uri_t *u, mulib_tsl_t *s)
{
  tls_fixture *f = p;
  const mulib_tsl_ops_t ops = {tls_handshake,tls_read,tls_write,tls_close};
  assert (u->scheme.length == 5 && u->has_port);
  ++f->connected;
  if (!mulib_tsl_init (s,&ops,p)) return 0;
  return f->mode != 1;
}
static void
https (void)
{
  const char *invalid[] = {"https://x:","https://x:0/","https://x:65536/",
                          "https://x:999999999999999999999/","https://u@x/","https://x/#fragment"};
  mulib_http_message_t response = {0}, before = {0};
  before.status = 599;
  for (size_t i = 0; i < sizeof (invalid)/sizeof (*invalid); ++i) {
    tls_fixture f = {0}; response = before;
    assert (mulib_https_request (invalid[i],"GET",NULL,0,NULL,0,NULL,tls_connect,&f,&response) == MULIB_WEBCLIENT_INVALID);
    assert (!f.connected && !f.closed && !memcmp (&response,&before,sizeof (response)));
  }
  for (int mode = 0; mode <= 5; ++mode) {
    tls_fixture f = {0};
    f.reply = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    f.verified = mode != 5; f.mode = mode; response = before;
    int rc = mulib_https_request ("HTTPS://example.test:443/a?q=1","GET",NULL,0,NULL,0,NULL,tls_connect,&f,&response);
    assert (f.connected == 1 && f.closed == 1);
    if (!mode) {
      mulib_http_message_t sent = {0}; size_t used;
      assert (rc == MULIB_WEBCLIENT_OK && response.body_length == 2 && !memcmp (response.body,"ok",2));
      assert (mulib_http_parse (f.sent.data,f.sent.length,0,NULL,&sent,&used) == MULIB_HTTP_OK);
      assert (used == f.sent.length && !strcmp (sent.target,"/a?q=1"));
      assert (!strcmp (mulib_http_header (&sent,"Host"),"example.test:443"));
      mulib_http_message_free (&sent); mulib_http_message_free (&response);
    } else {
      assert (rc == MULIB_WEBCLIENT_TRANSPORT && !memcmp (&response,&before,sizeof (response)));
      if (mode == 1 || mode == 2 || mode == 5) assert (!f.sent.length);
    }
    mulib_buffer_free (&f.sent);
  }
  {
    tls_fixture f = {0}; mulib_webclient_options_t options = {{0,0,0,0},10};
    f.reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"; f.verified = 1; response = before;
    assert (mulib_https_request ("https://x/","GET",NULL,0,NULL,0,&options,tls_connect,&f,&response) == MULIB_WEBCLIENT_LIMIT);
    assert (f.closed == 1 && !memcmp (&response,&before,sizeof (response)));
    mulib_buffer_free (&f.sent);
  }
}
int main (void)
{
  http1 (); http2 (); http3 (); https ();
  puts ("HTTP framing, limits, construction and TLS lifecycle: OK");
  return 0;
}
