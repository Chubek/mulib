/** @file mulib-websocket.h
 * @brief RFC 6455 opening-handshake helpers and binary-safe frame codec.
 * Define MULIB_IMPL_CDD_WEBSOCKET in one C TU. Transport, entropy, application
 * protocols and the connection lifecycle are caller supplied.
 */
#ifndef MULIB_WEBSOCKET_H
#define MULIB_WEBSOCKET_H
#include "mulib-base64.h"
#include "mulib-buffer.h"
#include "mulib-http.h"
#include "mulib-uri.h"
#include "mulib-utf8.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_CDD_WEBSOCKET_CONT = 0, MULIB_CDD_WEBSOCKET_TEXT = 1,
  MULIB_CDD_WEBSOCKET_BINARY = 2, MULIB_CDD_WEBSOCKET_CLOSE = 8,
  MULIB_CDD_WEBSOCKET_PING = 9, MULIB_CDD_WEBSOCKET_PONG = 10,
  MULIB_WEBSOCKET_CONT = MULIB_CDD_WEBSOCKET_CONT,
  MULIB_WEBSOCKET_TEXT = MULIB_CDD_WEBSOCKET_TEXT,
  MULIB_WEBSOCKET_BINARY = MULIB_CDD_WEBSOCKET_BINARY,
  MULIB_WEBSOCKET_CLOSE = MULIB_CDD_WEBSOCKET_CLOSE,
  MULIB_WEBSOCKET_PING = MULIB_CDD_WEBSOCKET_PING,
  MULIB_WEBSOCKET_PONG = MULIB_CDD_WEBSOCKET_PONG
};
typedef struct mulib_websocket_frame {
  unsigned opcode, fin, masked;
  unsigned char *payload; /* owned on read; may be NULL when length is zero */
  size_t length;
} mulib_cdd_websocket_frame_t;
typedef mulib_cdd_websocket_frame_t mulib_websocket_frame_t;

/** Zero-init before use; maintain one instance per direction. Members are
 * private state; only copy the complete object, or reset it to all zeroes. */
typedef struct mulib_cdd_websocket_sequence {
  size_t message_bytes;
  unsigned fragmented_opcode, closed, utf8_remaining;
  unsigned char utf8_lower, utf8_upper;
} mulib_cdd_websocket_sequence_t;

/** Use 16 fresh cryptographically secure random bytes; output 24 chars + NUL. */
int mulib_cdd_websocket_key(const unsigned char random16[16], char out[25]);
/** SHA-1 + base64 RFC 6455 calculation; key is canonical base64 of 16 bytes. */
int mulib_cdd_websocket_accept(const char *key, char out[29]);
/** Validate a parsed HTTP/1.1 101 response for a request without subprotocols.
 * Extensions and any unsolicited protocol are rejected. */
int mulib_cdd_websocket_validate_upgrade(const mulib_http_message_t *reply,
    const char *sent_key);
/** Like validate_upgrade, but permit the server to return the given offered
 * HTTP token verbatim (case-sensitive), or to omit a protocol. */
int mulib_cdd_websocket_validate_upgrade_protocol(const mulib_http_message_t *reply,
    const char *sent_key, const char *offered_protocol);
/** Make a malloc-owned HTTP/1.1 upgrade request at most 16384 bytes. Supply
 * Host authority (include nondefault port) and a '/'-prefixed origin target.
 * A subprotocol, if offered, is one HTTP token. No extensions are offered. */
char *mulib_cdd_websocket_upgrade_request(const char *host, const char *target,
    const char *key, const char *subprotocol);
/** Serialize a frame to initialized byte buffer. Client MUST set masked=1,
 * supply a fresh cryptographically random 4-byte mask; a server uses masked=0.
 * Maximum 16 MiB. Returns 1 on atomic append or 0 on failure. Payload and mask
 * may alias previously written bytes in out. */
int mulib_cdd_websocket_frame_write(mulib_buffer_t *out, unsigned opcode, int fin,
    int masked, const unsigned char mask[4], const void *payload, size_t length);
/** Parse one frame prefix. 1 success; 0 incomplete; -1 malformed;
 * -2 limit; -3 OOM. expected_mask: 0 server->client, 1 client->server,
 * -1 either (offline use). out and consumed unchanged unless successful;
 * consumed may be NULL. out must not already own a frame. NULL input is valid
 * only at zero length. Zero max_length selects 16 MiB. Rejects RSV bits,
 * noncanonical lengths, invalid Close bodies, and invalid complete text. */
int mulib_cdd_websocket_frame_read(const void *wire, size_t length, size_t max_length,
    int expected_mask, mulib_cdd_websocket_frame_t *out, size_t *consumed);
void mulib_cdd_websocket_frame_free(mulib_cdd_websocket_frame_t *frame);
/** Check and record one decoded frame in wire order, without buffering a
 * message. Checks continuations, UTF-8 across text fragments, and rejects data
 * after a Close in this direction. 1 accepted, -1 invalid, -2 cumulative data
 * payload limit (zero max_message_length selects 16 MiB). On error state is
 * unchanged; caller must fail the connection. No automatic control replies. */
int mulib_cdd_websocket_sequence_check(mulib_cdd_websocket_sequence_t *state,
    const mulib_cdd_websocket_frame_t *frame, size_t max_message_length);

/* Legacy source/link compatibility. */
int mulib_websocket_key(const unsigned char random16[16], char out[25]);
int mulib_websocket_accept(const char *key, char out[29]);
int mulib_websocket_validate_upgrade(const mulib_http_message_t *reply,
    const char *sent_key);
char *mulib_websocket_upgrade_request(const char *host, const char *target,
    const char *key, const char *subprotocol);
int mulib_websocket_frame_write(mulib_buffer_t *out, unsigned opcode, int fin,
    int masked, const unsigned char mask[4], const void *payload, size_t length);
int mulib_websocket_frame_read(const void *wire, size_t length, size_t max_length,
    int expected_mask, mulib_websocket_frame_t *out, size_t *consumed);
void mulib_websocket_frame_free(mulib_websocket_frame_t *frame);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBSOCKET_H */

#if defined(MULIB_IMPL_CDD_WEBSOCKET) || defined(MULIB_IMPL_WEBSOCKET)
#ifndef MULIB_WEBSOCKET_IMPLEMENTED
#define MULIB_WEBSOCKET_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define MULIB_WS_DEFAULT_LIMIT_ ((size_t)16777216)
static uint32_t mulib_ws_rol_(uint32_t x,unsigned n) {return (x<<n)|(x>>(32-n));}
/* RFC 3174 SHA-1 for one known-size (60-byte) handshake input. */
static void mulib_ws_sha1_(const unsigned char input[60],unsigned char digest[20]) {
  unsigned char blocks[128]={0};uint32_t h[5]={0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u,0xc3d2e1f0u};
  size_t b;memcpy(blocks,input,60);blocks[60]=0x80;blocks[126]=1;blocks[127]=0xe0;
  for(b=0;b<2;b++) {
    uint32_t w[80],a,c,d,e,f,k,t,bb;
    for(int i=0;i<16;i++) {
      const unsigned char *p=blocks+b*64+i*4;
      w[i]=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    }
    for(int i=16;i<80;i++) w[i]=mulib_ws_rol_(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=h[0];bb=h[1];c=h[2];d=h[3];e=h[4];
    for(int i=0;i<80;i++) {
      if(i<20){f=(bb&c)|((~bb)&d);k=0x5a827999u;}
      else if(i<40){f=bb^c^d;k=0x6ed9eba1u;}
      else if(i<60){f=(bb&c)|(bb&d)|(c&d);k=0x8f1bbcdcu;}
      else{f=bb^c^d;k=0xca62c1d6u;}
      t=mulib_ws_rol_(a,5)+f+e+k+w[i];e=d;d=c;c=mulib_ws_rol_(bb,30);bb=a;a=t;
    }
    h[0]+=a;h[1]+=bb;h[2]+=c;h[3]+=d;h[4]+=e;
  }
  for(int i=0;i<5;i++)for(int j=0;j<4;j++)digest[i*4+j]=(unsigned char)(h[i]>>(24-8*j));
}
int mulib_cdd_websocket_key(const unsigned char random16[16],char out[25]) {
  unsigned char source[16];
  if(!random16||!out)return 0;
  memcpy(source, random16, sizeof source);
  return mulib_base64_encode(source, sizeof source, out, 25) == 24;
}
static int mulib_ws_valid_key_(const char *key) {
  unsigned char decoded[16];
  size_t n = 0;
  if (!key) return 0;
  for (size_t i = 0; i < 24; ++i) if (!key[i]) return 0;
  if (key[24] || key[22] != '=' || key[23] != '='
      || !strchr("AQgw", key[21])) return 0;
  return mulib_base64_decode(key, 24, decoded, sizeof decoded, &n) && n == sizeof decoded;
}
int mulib_cdd_websocket_accept(const char *key,char out[29]) {
  static const char guid[]="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char input[60],digest[20];
  if (!out || !mulib_ws_valid_key_(key)) return 0;
  memcpy(input,key,24);memcpy(input+24,guid,36);
  mulib_ws_sha1_(input,digest);
  return mulib_base64_encode(digest,20,out,29)==28;
}
static int mulib_ws_equal_(const char *s, size_t n, const char *lowercase) {
  size_t i = 0;
  if (!s) return 0;
  for (; i < n && lowercase[i]; ++i) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
    if (c != (unsigned char)lowercase[i]) return 0;
  }
  return i == n && !lowercase[i];
}
static const char *mulib_ws_ows_(const char *s) {
  while (*s == ' ' || *s == '\t') ++s;
  return s;
}
static const char *mulib_ws_token_end_(const char *s) {
  for (; *s; ++s) {
    unsigned char c = (unsigned char)*s;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
          || (c >= '0' && c <= '9') || strchr("!#$%&'*+-.^_`|~", c)))
      break;
  }
  return s;
}
static int mulib_ws_connection_(const char *s, int *found) {
  if (!s) return 0;
  while (*(s = mulib_ws_ows_(s))) {
    const char *end;
    if (*s == ',') { ++s; continue; } /* ignore empty HTTP list elements */
    end = mulib_ws_token_end_(s);
    if (end == s) return 0;
    if (mulib_ws_equal_(s, (size_t)(end - s), "upgrade"))
      *found = 1;
    s = mulib_ws_ows_(end);
    if (*s && *s++ != ',') return 0;
  }
  return 1;
}
static int mulib_ws_accept_matches_(const char *s, const char expected[29]) {
  unsigned diff = 0;
  if (!s) return 0;
  s = mulib_ws_ows_(s);
  for (size_t i = 0; i < 28; ++i) {
    if (!s[i]) return 0;
    diff |= (unsigned char)(expected[i] ^ s[i]);
  }
  return diff == 0 && !*mulib_ws_ows_(s + 28);
}
static int mulib_ws_validate_upgrade_(const mulib_http_message_t *r, const char *key,
    const char *offer, int legacy) {
  char expected[29];
  int upgrade = 0, connection = 0, accept = 0, protocol = 0;
  if (!r || !r->is_response || r->status != 101 || r->body_length
      || r->chunked || r->close_delimited || r->trailer_count
      || (!r->headers && r->header_count)
      || !mulib_cdd_websocket_accept(key, expected)
      || (offer && (!*offer || *mulib_ws_token_end_(offer)))) return 0;
  for (size_t i = 0; i < r->header_count; ++i) {
    const mulib_http_header_t *h = &r->headers[i];
    size_t n;
    if (!h->name || !h->value) return 0;
    n = strlen(h->name);
    if (mulib_ws_equal_(h->name, n, "upgrade")) {
      const char *s = mulib_ws_ows_(h->value), *end = mulib_ws_token_end_(s);
      if (upgrade || !mulib_ws_equal_(s, (size_t)(end - s), "websocket")
          || *mulib_ws_ows_(end)) return 0;
      upgrade = 1;
    } else if (mulib_ws_equal_(h->name, n, "connection")) {
      if (!mulib_ws_connection_(h->value, &connection)) return 0;
    } else if (mulib_ws_equal_(h->name, n, "sec-websocket-accept")) {
      if (accept || !mulib_ws_accept_matches_(h->value, expected)) return 0;
      accept = 1;
    } else if (mulib_ws_equal_(h->name, n, "sec-websocket-protocol")) {
      const char *s = mulib_ws_ows_(h->value), *end = mulib_ws_token_end_(s);
      if (protocol || s == end || *mulib_ws_ows_(end)) return 0;
      if (!legacy && (!offer || strlen(offer) != (size_t)(end - s)
          || memcmp(s, offer, (size_t)(end - s)))) return 0;
      protocol = 1;
    } else if (mulib_ws_equal_(h->name, n, "sec-websocket-extensions")
        || mulib_ws_equal_(h->name, n, "content-length")
        || mulib_ws_equal_(h->name, n, "transfer-encoding")) return 0;
  }
  return upgrade && connection && accept;
}
int mulib_cdd_websocket_validate_upgrade_protocol(const mulib_http_message_t *r,
    const char *key, const char *offer) {
  return mulib_ws_validate_upgrade_(r, key, offer, 0);
}
int mulib_cdd_websocket_validate_upgrade(const mulib_http_message_t *r, const char *key) {
  return mulib_ws_validate_upgrade_(r, key, NULL, 0);
}
/* Return 0 for absent, empty, or overlong required strings. */
static size_t mulib_ws_request_len_(const char *s) {
  size_t n = 0;
  if (!s) return 0;
  while (n < 16384 && s[n]) ++n;
  return s[n] ? 0 : n;
}
static int mulib_ws_authority_(char *prefix, const char *host, size_t n) {
  mulib_uri_t u;
  unsigned port = 0;
  prefix[0] = prefix[1] = '/';
  memcpy(prefix + 2, host, n);
  if (!mulib_uri_parse(prefix, n + 2, &u) || !u.has_authority || u.has_userinfo
      || u.path.length || u.has_query || u.has_fragment || !u.host.length)
    return 0;
  if (u.has_port) {
    if (!u.port.length) return 0;
    for (size_t i = 0; i < u.port.length; ++i) {
      unsigned digit = (unsigned)(u.port.data[i] - '0');
      if (digit > 9 || port > (65535u - digit) / 10u) return 0;
      port = port * 10u + digit;
    }
    if (!port) return 0;
  }
  return 1;
}
char *mulib_cdd_websocket_upgrade_request(const char *host, const char *target,
    const char *key, const char *protocol) {
  static const char fmt[]="GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\n"
      "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n%s%s%s\r\n";
  size_t h = mulib_ws_request_len_(host), t = mulib_ws_request_len_(target);
  mulib_uri_t u;
  char *out;
  int n;
  if (!h || !t || *target != '/' || !mulib_ws_valid_key_(key)
      || (protocol && (!mulib_ws_request_len_(protocol) || *mulib_ws_token_end_(protocol)))
      || !mulib_uri_parse(target, t, &u) || u.has_authority || u.has_fragment)
    return NULL;
  n=snprintf(NULL,0,fmt,target,host,key,protocol?"Sec-WebSocket-Protocol: ":"",
      protocol?protocol:"",protocol?"\r\n":"");
  if(n<0 || n>16384)return NULL;
  out=(char *)malloc((size_t)n+1);if(!out)return NULL;
  if (!mulib_ws_authority_(out, host, h)
      || snprintf(out,(size_t)n+1,fmt,target,host,key,protocol?"Sec-WebSocket-Protocol: ":"",
      protocol?protocol:"",protocol?"\r\n":"") != n) {
    free(out);
    return NULL;
  }
  return out;
}
static int mulib_ws_opcode_(unsigned opcode) {
  return opcode==0||opcode==1||opcode==2||opcode==8||opcode==9||opcode==10;
}
static int mulib_ws_payload_(unsigned opcode, unsigned fin, unsigned masked,
    const unsigned char *p, size_t len) {
  if (opcode == MULIB_CDD_WEBSOCKET_CLOSE && len) {
    unsigned code;
    if (len == 1) return 0;
    code = ((unsigned)p[0] << 8) | p[1];
    if (!((code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1014)
          || (code >= 3000 && code <= 4999))
        || (code == 1010 && !masked)) return 0;
    return mulib_utf8_validate(p + 2, len - 2);
  }
  return opcode != MULIB_CDD_WEBSOCKET_TEXT || !fin || mulib_utf8_validate(p, len);
}
void mulib_cdd_websocket_frame_free(mulib_cdd_websocket_frame_t *f) {
  if (f) {
    free(f->payload);
    memset(f, 0, sizeof(*f));
  }
}
int mulib_cdd_websocket_frame_write(mulib_buffer_t *out,unsigned opcode,int fin,
    int masked,const unsigned char mask[4],const void *payload,size_t len) {
  unsigned char *wire;
  size_t offset = 2, head, total;
  int ok;
  if (!out || !mulib_ws_opcode_(opcode) || (fin != 0 && fin != 1)
      || (masked != 0 && masked != 1) || (masked && !mask) || (!payload && len)
      || out->length > out->capacity || (!out->data && out->capacity)
      || len > MULIB_WS_DEFAULT_LIMIT_ || (opcode >= 8 && (!fin || len > 125)))
    return 0;
  head = 2u + (len >= 126 ? (len <= 65535 ? 2u : 8u) : 0u) + (masked ? 4u : 0u);
  if (len > SIZE_MAX - head || head + len > SIZE_MAX - out->length
      || !mulib_ws_payload_(opcode, (unsigned)fin, (unsigned)masked,
          (const unsigned char *)payload, len)) return 0;
  total = head + len;
  wire = (unsigned char *)malloc(total);
  if (!wire) return 0;
  wire[0] = (unsigned char)((fin ? 0x80u : 0u) | opcode);
  wire[1] = (unsigned char)((masked ? 0x80u : 0u) | (len < 126 ? len : len <= 65535 ? 126u : 127u));
  if (len >= 126 && len <= 65535) {
    wire[offset++] = (unsigned char)(len >> 8);
    wire[offset++] = (unsigned char)len;
  } else if (len > 65535) {
    for (int i = 7; i >= 0; --i)
      wire[offset++] = (unsigned char)((uint64_t)len >> (i * 8));
  }
  if (masked) {
    memcpy(wire + offset, mask, 4);
    offset += 4;
    for (size_t i = 0; i < len; ++i)
      wire[offset + i] = (unsigned char)(((const unsigned char *)payload)[i] ^ mask[i % 4]);
  } else if (len) memcpy(wire + offset, payload, len);
  ok = mulib_buffer_append(out, wire, total);
  free(wire);
  return ok;
}
int mulib_cdd_websocket_frame_read(const void *input, size_t n, size_t limit, int expect,
    mulib_cdd_websocket_frame_t *out, size_t *consumed) {
  const unsigned char *p = (const unsigned char *)input, *mask = NULL;
  size_t header = 2, size;
  uint64_t len;
  unsigned opcode, masked, fin;
  mulib_cdd_websocket_frame_t tmp = {0};
  if (!out || (!p && n) || (expect != -1 && expect != 0 && expect != 1)) return -1;
  if (!n) return 0;
  opcode = p[0] & 0xfu;
  fin = p[0] >> 7;
  if ((p[0] & 0x70u) || !mulib_ws_opcode_(opcode) || (opcode >= 8 && !fin)) return -1;
  if (n < 2) return 0;
  masked = p[1] >> 7;
  if (expect >= 0 && masked != (unsigned)expect) return -1;
  len = p[1] & 127u;
  if ((opcode >= 8 && len >= 126) || (opcode == MULIB_CDD_WEBSOCKET_CLOSE && len == 1))
    return -1;
  if (len == 126) {
    if (n < 4) return 0;
    len = ((uint64_t)p[2] << 8) | p[3];
    if (len < 126) return -1;
    header = 4;
  } else if (len == 127) {
    if (n >= 3 && (p[2] & 0x80u)) return -1;
    if (n < 10) return 0;
    len = 0;
    for (size_t i = 2; i < 10; ++i) len = (len << 8) | p[i];
    if (len <= 65535) return -1;
    header = 10;
  }
  if (!limit) limit = MULIB_WS_DEFAULT_LIMIT_;
  if (masked) header += 4;
  if (len > SIZE_MAX - header || len > limit) return -2;
  size = (size_t)len;
  if (n < header || size > n - header) return 0;
  if (masked) mask = p + header - 4;
  if (size) {
    if (!mask && !mulib_ws_payload_(opcode, fin, masked, p + header, size)) return -1;
    tmp.payload = (unsigned char *)malloc(size);
    if (!tmp.payload) return -3;
    if (mask) {
      for (size_t i = 0; i < size; ++i)
        tmp.payload[i] = (unsigned char)(p[header + i] ^ mask[i % 4]);
      if (!mulib_ws_payload_(opcode, fin, masked, tmp.payload, size)) {
        free(tmp.payload);
        return -1;
      }
    } else memcpy(tmp.payload, p + header, size);
  }
  tmp.length = size;
  tmp.opcode = opcode;
  tmp.fin = fin;
  tmp.masked = masked;
  *out = tmp;
  if (consumed) *consumed = header + size;
  return 1;
}
static int mulib_ws_text_(mulib_cdd_websocket_sequence_t *s,
    const unsigned char *p, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = p[i];
    if (s->utf8_remaining) {
      if (c < s->utf8_lower || c > s->utf8_upper) return 0;
      --s->utf8_remaining;
      s->utf8_lower = 0x80;
      s->utf8_upper = 0xbf;
    } else if (c >= 0x80) {
      if (c < 0xc2 || c > 0xf4) return 0;
      s->utf8_remaining = c < 0xe0 ? 1u : c < 0xf0 ? 2u : 3u;
      s->utf8_lower = c == 0xe0 ? 0xa0 : c == 0xf0 ? 0x90 : 0x80;
      s->utf8_upper = c == 0xed ? 0x9f : c == 0xf4 ? 0x8f : 0xbf;
    }
  }
  return 1;
}
int mulib_cdd_websocket_sequence_check(mulib_cdd_websocket_sequence_t *state,
    const mulib_cdd_websocket_frame_t *f, size_t limit) {
  mulib_cdd_websocket_sequence_t next;
  unsigned opcode;
  if (!state || !f || !mulib_ws_opcode_(f->opcode) || f->fin > 1 || f->masked > 1
      || (!f->payload && f->length)) return -1;
  if (f->opcode >= 8) {
    if (!f->fin || f->length > 125
        || !mulib_ws_payload_(f->opcode, f->fin, f->masked, f->payload, f->length)) return -1;
    if (f->opcode == MULIB_CDD_WEBSOCKET_CLOSE) {
      if (state->closed) return -1;
      memset(state, 0, sizeof(*state));
      state->closed = 1;
    }
    return 1;
  }
  if (state->closed || (!f->opcode && !state->fragmented_opcode)
      || (f->opcode && state->fragmented_opcode)) return -1;
  if (!limit) limit = MULIB_WS_DEFAULT_LIMIT_;
  if (state->message_bytes > limit || f->length > limit - state->message_bytes) return -2;
  next = *state;
  opcode = f->opcode ? f->opcode : state->fragmented_opcode;
  if (opcode == MULIB_CDD_WEBSOCKET_TEXT
      && (!mulib_ws_text_(&next, f->payload, f->length) || (f->fin && next.utf8_remaining)))
    return -1;
  if (f->fin) memset(&next, 0, sizeof next);
  else {
    next.fragmented_opcode = opcode;
    next.message_bytes += f->length;
  }
  *state = next;
  return 1;
}

int mulib_websocket_key(const unsigned char random16[16], char out[25]) {
  return mulib_cdd_websocket_key(random16, out);
}
int mulib_websocket_accept(const char *key, char out[29]) {
  return mulib_cdd_websocket_accept(key, out);
}
int mulib_websocket_validate_upgrade(const mulib_http_message_t *r, const char *key) {
  return mulib_ws_validate_upgrade_(r, key, NULL, 1);
}
char *mulib_websocket_upgrade_request(const char *host, const char *target,
    const char *key, const char *protocol) {
  return mulib_cdd_websocket_upgrade_request(host, target, key, protocol);
}
int mulib_websocket_frame_write(mulib_buffer_t *out, unsigned opcode, int fin,
    int masked, const unsigned char mask[4], const void *payload, size_t len) {
  return mulib_cdd_websocket_frame_write(out, opcode, fin, masked, mask, payload, len);
}
int mulib_websocket_frame_read(const void *input, size_t n, size_t limit, int expect,
    mulib_websocket_frame_t *out, size_t *consumed) {
  return mulib_cdd_websocket_frame_read(input, n, limit, expect, out, consumed);
}
void mulib_websocket_frame_free(mulib_websocket_frame_t *f) {
  mulib_cdd_websocket_frame_free(f);
}
#endif
#endif
