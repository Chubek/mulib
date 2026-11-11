/** @file mulib-tls.h
 * @brief Native authenticated TLS 1.3 client (X25519/AES-128-GCM/SHA-256).
 * Define MULIB_IMPL_TLS and dependencies listed in manual/mulib-tls.md.
 * Blocking TSL adapter over bounded POSIX socket waits; no external TLS library. */
#ifndef MULIB_TLS_H
#define MULIB_TLS_H
#include "mulib-tsl.h"
#include "mulib-uri.h"
#include "mulib-socket.h"
#include "mulib-x509.h"
#include "mulib-x25519.h"
#include "mulib-hkdf.h"
#include "mulib-aes-gcm.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_tls_options {
  const mulib_x509_store_t *trust; /* borrowed, overrides ca_file */
  const char *ca_file; /* NULL selects system trust bundle */
  unsigned timeout_ms; /* zero selects 30000; each socket wait */
} mulib_tls_options_t;
/** HTTPS connector, directly usable as mulib_https_connect_fn. user is a
 * borrowed mulib_tls_options_t or NULL. Creates an owning, unhandshaken TSL
 * adapter in zero-state output. Returns 1/0. The adapter must be closed.
 * Verifies chain, host, CertificateVerify and Finished before any application
 * bytes. ALPN offers only http/1.1. No TLS 1.2/PSK/0-RTT/client certificates.
 * TCP EOF without close_notify is an error. */
int mulib_tls_connect (void *user, const mulib_uri_t *endpoint, mulib_tsl_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TLS) && !defined(MULIB_TLS_IMPLEMENTED)
#define MULIB_TLS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define MULIB_TLS_HAND_MAX_ 262144
typedef struct mulib_tls_cursor_ { const unsigned char *p; size_t n; } mulib_tls_cursor_;
typedef struct mulib_tls_keys_ {
  mulib_aes_gcm_t aes; unsigned char iv[12], secret[32]; uint64_t sequence;
} mulib_tls_keys_;
typedef struct mulib_tls_session_ {
  mulib_socket_t socket;
  mulib_x509_store_t owned_trust;
  const mulib_x509_store_t *trust;
  char host[254]; unsigned timeout, ready, closed, ccs, sni;
  unsigned char random[32], session_id[32], private_key[32], public_key[32];
  mulib_sha_ctx transcript;
  mulib_tls_keys_ rx,tx;
  unsigned char record[16640], hand[MULIB_TLS_HAND_MAX_];
  size_t record_length, record_offset, hand_length, handshake_bytes;
} mulib_tls_session_;
static unsigned mulib_tls_u16_ (const unsigned char *p) { return ((unsigned)p[0]<<8)|p[1]; }
static size_t mulib_tls_u24_ (const unsigned char *p) { return ((size_t)p[0]<<16)|((size_t)p[1]<<8)|p[2]; }
static void mulib_tls_put16_ (unsigned char *p,size_t n) { p[0]=(unsigned char)(n>>8);p[1]=(unsigned char)n; }
static void mulib_tls_put24_ (unsigned char *p,size_t n) { p[0]=(unsigned char)(n>>16);p[1]=(unsigned char)(n>>8);p[2]=(unsigned char)n; }
static int mulib_tls_take_ (mulib_tls_cursor_ *c,size_t n,const unsigned char **p)
{ if(n>c->n)return 0;*p=c->p;c->p+=n;c->n-=n;return 1; }
static int mulib_tls_vec_ (mulib_tls_cursor_ *c,unsigned width,mulib_tls_cursor_ *v)
{
  const unsigned char *p;size_t n=0;
  if(!mulib_tls_take_(c,width,&p))return 0;
  for(unsigned i=0;i<width;++i)n=(n<<8)|p[i];
  if(!mulib_tls_take_(c,n,&p))return 0;
  v->p=p;v->n=n;return 1;
}
static int mulib_tls_io_ (mulib_tls_session_ *s,void *buffer,size_t n,int write)
{
  unsigned char *p=(unsigned char *)buffer;
  while(n) {
    size_t got=0;int rc;
    if(mulib_socket_wait(&s->socket,write,s->timeout))return 0;
    rc=write?mulib_socket_send(&s->socket,p,n,&got):mulib_socket_receive(&s->socket,p,n,&got);
    if(rc==MULIB_SOCKET_AGAIN)continue;
    if(rc||!got)return 0;
    p+=got;n-=got;
  }
  return 1;
}
static void mulib_tls_nonce_ (const mulib_tls_keys_ *k,unsigned char out[12])
{ memcpy(out,k->iv,12);for(unsigned i=0;i<8;++i)out[11-i]^=(unsigned char)(k->sequence>>(8*i)); }
static int mulib_tls_send_ (mulib_tls_session_ *s,unsigned type,const void *data,size_t n,int encrypted)
{
  unsigned char wire[16406],nonce[12];size_t wn=n;
  if(n>16384)return 0;
  wire[0]=(unsigned char)(encrypted?23:type);wire[1]=3;wire[2]=3;
  if(n)memcpy(wire+5,data,n);
  if(encrypted) {
    if(s->tx.sequence>=UINT64_C(8388608))return 0;
    wire[5+n]=(unsigned char)type;wn=n+17;mulib_tls_put16_(wire+3,wn);mulib_tls_nonce_(&s->tx,nonce);
    if(!mulib_aes_gcm_seal(&s->tx.aes,nonce,wire,5,wire+5,n+1,wire+5,wire+6+n))return 0;
    ++s->tx.sequence;
  } else mulib_tls_put16_(wire+3,wn);
  {int ok=mulib_tls_io_(s,wire,wn+5,1);mulib_secret_wipe(wire,sizeof wire);return ok;}
}
static int mulib_tls_record_ (mulib_tls_session_ *s,int encrypted,unsigned *type)
{
  unsigned char header[5],nonce[12];size_t n;
  for(unsigned attempts=0;attempts<8;++attempts) {
    if(!mulib_tls_io_(s,header,5,0)||header[1]!=3||header[2]!=3)return 0;
    n=mulib_tls_u16_(header+3);
    if(!n||n>sizeof s->record||(!encrypted&&n>16384)||!mulib_tls_io_(s,s->record,n,0))return 0;
    if(header[0]==20&&!s->ready) {
      if(n!=1||s->record[0]!=1||++s->ccs>2)return 0;
      continue;
    }
    if(encrypted) {
      if(header[0]!=23||n<17||s->rx.sequence>=UINT64_C(8388608))return 0;
      mulib_tls_nonce_(&s->rx,nonce);
      if(!mulib_aes_gcm_open(&s->rx.aes,nonce,header,5,s->record,n-16,s->record+n-16,s->record))return 0;
      ++s->rx.sequence;n-=16;
      while(n&&!s->record[n-1])--n;
      if(!n)return 0;
      *type=s->record[--n];if(n>16384)return 0;
    } else *type=header[0];
    s->record_length=n;s->record_offset=0;
    if(*type==21) {
      if(n!=2||s->record[0]<1||s->record[0]>2)return 0;
      if(encrypted&&s->ready&&s->record[1]==0&&!s->hand_length) {s->closed=1;return 1;}
      return 0;
    }
    return *type==22||(*type==23&&s->ready);
  }
  return 0;
}
static int mulib_tls_message_ (mulib_tls_session_ *s,int encrypted,size_t *length)
{
  for(unsigned attempts=0;attempts<256;++attempts) {
    if(s->hand_length>=4) {
      size_t n=mulib_tls_u24_(s->hand+1)+4;
      if(n>sizeof s->hand)return 0;
      if(s->hand_length>=n) {*length=n;return 1;}
    }
    {unsigned type;
      if(!mulib_tls_record_(s,encrypted,&type)||type!=22||s->record_length>sizeof s->hand-s->hand_length
          ||s->record_length>1048576-s->handshake_bytes)return 0;
      memcpy(s->hand+s->hand_length,s->record,s->record_length);s->hand_length+=s->record_length;s->handshake_bytes+=s->record_length;
    }
  }
  return 0;
}
static void mulib_tls_consume_ (mulib_tls_session_ *s,size_t n,int transcript)
{
  if(transcript)(void)mulib_sha_update(&s->transcript,s->hand,n);
  s->hand_length-=n;memmove(s->hand,s->hand+n,s->hand_length);
}
static void mulib_tls_hash_ (mulib_tls_session_ *s,unsigned char hash[32])
{mulib_sha_ctx copy=s->transcript;(void)mulib_sha_final(&copy,hash,32);}
static int mulib_tls_keys_derive_ (mulib_tls_keys_ *k,const unsigned char secret[32])
{
  unsigned char key[16];int ok;
  memset(k,0,sizeof *k);memcpy(k->secret,secret,32);
  ok=mulib_hkdf_tls13(secret,"key",NULL,0,key,16)&&mulib_hkdf_tls13(secret,"iv",NULL,0,k->iv,12)&&mulib_aes_gcm_init(&k->aes,key);
  mulib_secret_wipe(key,sizeof key);return ok;
}
static int mulib_tls_hello_ (mulib_tls_session_ *s)
{
  unsigned char hello[512];size_t n=4,ext,hn=strlen(s->host);mulib_inet4_t ip4;mulib_inet6_t ip6;
  static const unsigned char fixed[]={0,43,0,3,2,3,4, 0,10,0,4,0,2,0,29,
    0,13,0,12,0,10,8,4,8,5,8,6,4,3,5,3,
    0,50,0,18,0,16,4,1,5,1,6,1,8,4,8,5,8,6,4,3,5,3,
    0,16,0,11,0,9,8,'h','t','t','p','/','1','.','1'};
  if(!mulib_entropy(s->random,32)||!mulib_entropy(s->session_id,32)||!mulib_entropy(s->private_key,32)
      ||!mulib_x25519_public(s->public_key,s->private_key))return 0;
  hello[n++]=3;hello[n++]=3;memcpy(hello+n,s->random,32);n+=32;hello[n++]=32;memcpy(hello+n,s->session_id,32);n+=32;
  hello[n++]=0;hello[n++]=2;hello[n++]=0x13;hello[n++]=1;hello[n++]=1;hello[n++]=0;
  ext=n;n+=2;
  s->sni=!mulib_inet4_parse(s->host,hn,&ip4)&&!mulib_inet6_parse(s->host,hn,&ip6);
  if(s->sni) {
    mulib_tls_put16_(hello+n,0);mulib_tls_put16_(hello+n+2,hn+5);mulib_tls_put16_(hello+n+4,hn+3);hello[n+6]=0;mulib_tls_put16_(hello+n+7,hn);n+=9;memcpy(hello+n,s->host,hn);n+=hn;
  }
  memcpy(hello+n,fixed,sizeof fixed);n+=sizeof fixed;
  mulib_tls_put16_(hello+n,51);mulib_tls_put16_(hello+n+2,38);mulib_tls_put16_(hello+n+4,36);mulib_tls_put16_(hello+n+6,29);mulib_tls_put16_(hello+n+8,32);memcpy(hello+n+10,s->public_key,32);n+=42;
  mulib_tls_put16_(hello+ext,n-ext-2);hello[0]=1;mulib_tls_put24_(hello+1,n-4);
  (void)mulib_sha_init(&s->transcript,MULIB_SHA256);(void)mulib_sha_update(&s->transcript,hello,n);
  return mulib_tls_send_(s,22,hello,n,0);
}
static int mulib_tls_serverhello_ (mulib_tls_session_ *s,size_t n,unsigned char shared[32])
{
  static const unsigned char retry[32]={0xcf,0x21,0xad,0x74,0xe5,0x9a,0x61,0x11,0xbe,0x1d,0x8c,0x02,0x1e,0x65,0xb8,0x91,0xc2,0xa2,0x11,0x16,0x7a,0xbb,0x8c,0x5e,0x07,0x9e,0x09,0xe2,0xc8,0xa8,0x33,0x9c};
  mulib_tls_cursor_ c={s->hand+4,n-4},sid,extensions,v;const unsigned char *p;unsigned seen=0;
  if(s->hand[0]!=2||!mulib_tls_take_(&c,34,&p)||p[0]!=3||p[1]!=3||!memcmp(p+2,retry,32)
      ||!mulib_tls_vec_(&c,1,&sid)||sid.n!=32||memcmp(sid.p,s->session_id,32)||!mulib_tls_take_(&c,3,&p)
      ||p[0]!=0x13||p[1]!=1||p[2]||!mulib_tls_vec_(&c,2,&extensions)||c.n)return 0;
  while(extensions.n) {
    unsigned id;if(!mulib_tls_take_(&extensions,2,&p)||!mulib_tls_vec_(&extensions,2,&v))return 0;id=mulib_tls_u16_(p);
    if(id==43&&!(seen&1)) {if(v.n!=2||v.p[0]!=3||v.p[1]!=4)return 0;seen|=1;}
    else if(id==51&&!(seen&2)) {if(v.n!=36||mulib_tls_u16_(v.p)!=29||mulib_tls_u16_(v.p+2)!=32||!mulib_x25519(shared,s->private_key,v.p+4))return 0;seen|=2;}
    else return 0;
  }
  return seen==3;
}
static int mulib_tls_extensions_ (mulib_tls_session_ *s,size_t n)
{
  mulib_tls_cursor_ c={s->hand+4,n-4},ext,v;const unsigned char *p;unsigned seen=0;
  if(s->hand[0]!=8||!mulib_tls_vec_(&c,2,&ext)||c.n)return 0;
  while(ext.n) {
    unsigned id,bit;
    if(!mulib_tls_take_(&ext,2,&p)||!mulib_tls_vec_(&ext,2,&v))return 0;
    id=mulib_tls_u16_(p);bit=id==0?1:id==16?2:id==10?4:0;
    if(!bit||(seen&bit))return 0;
    seen|=bit;
    if(id==0) {if(!s->sni||v.n)return 0;}
    else if(id==16) {if(v.n!=11||memcmp(v.p,"\x00\x09\x08http/1.1",11))return 0;}
    else {mulib_tls_cursor_ groups;if(!mulib_tls_vec_(&v,2,&groups)||v.n||!groups.n||(groups.n&1))return 0;}
  }
  return 1;
}
static int mulib_tls_certrequest_ (mulib_tls_session_ *s,size_t n)
{
  mulib_tls_cursor_ c={s->hand+4,n-4},context,ext,v;const unsigned char *p;unsigned ids[64],count=0,has_sig=0;
  if(!mulib_tls_vec_(&c,1,&context)||context.n||!mulib_tls_vec_(&c,2,&ext)||c.n)return 0;
  while(ext.n) {
    unsigned id;if(count==64||!mulib_tls_take_(&ext,2,&p)||!mulib_tls_vec_(&ext,2,&v))return 0;id=mulib_tls_u16_(p);
    for(unsigned i=0;i<count;++i)if(ids[i]==id)return 0;
    ids[count++]=id;
    if(id==13) {mulib_tls_cursor_ algs;if(!mulib_tls_vec_(&v,2,&algs)||v.n||!algs.n||(algs.n&1))return 0;has_sig=1;}
  }
  return has_sig;
}
static int mulib_tls_certificates_ (mulib_tls_session_ *s,const unsigned char *message,size_t n,mulib_public_key_t *leaf)
{
  mulib_tls_cursor_ c={message+4,n-4},context,list,der,ext;mulib_x509_certificate_t chain[10];size_t count=0;time_t now;
  if(message[0]!=11||!mulib_tls_vec_(&c,1,&context)||context.n||!mulib_tls_vec_(&c,3,&list)||c.n)return 0;
  while(list.n) {
    if(count==10||!mulib_tls_vec_(&list,3,&der)||!mulib_x509_parse(der.p,der.n,chain+count)
        ||!mulib_tls_vec_(&list,2,&ext)||ext.n)return 0;
    ++count;
  }
  now=time(NULL);
  if(now==(time_t)-1||!mulib_x509_verify(chain,count,s->trust,s->host,(int64_t)now))return 0;
  *leaf=chain[0].public_key;return 1;
}
static int mulib_tls_certverify_ (mulib_tls_session_ *s,size_t n,const mulib_public_key_t *key)
{
  unsigned char input[130];unsigned scheme;mulib_tls_cursor_ c={s->hand+4,n-4},sig;const unsigned char *p;
  if(s->hand[0]!=15||!mulib_tls_take_(&c,2,&p)||!mulib_tls_vec_(&c,2,&sig)||c.n)return 0;
  scheme=mulib_tls_u16_(p);
  if(!((scheme>=0x0804&&scheme<=0x0806&&key->kind==1)||(scheme==0x0403&&key->kind==23)||(scheme==0x0503&&key->kind==24)))return 0;
  memset(input,32,64);memcpy(input+64,"TLS 1.3, server CertificateVerify",33);input[97]=0;mulib_tls_hash_(s,input+98);
  return mulib_signature_verify(key,scheme,input,sizeof input,sig.p,sig.n);
}
static int mulib_tls_finished_ (mulib_tls_session_ *s,const unsigned char secret[32],unsigned char out[32])
{
  unsigned char key[32],hash[32];int ok;
  mulib_tls_hash_(s,hash);ok=mulib_hkdf_tls13(secret,"finished",NULL,0,key,32)&&mulib_hmac(MULIB_SHA256,key,32,hash,32,out,32);
  mulib_secret_wipe(key,sizeof key);return ok;
}
static int mulib_tls_handshake_ (void *context,int *verified)
{
  mulib_tls_session_ *s=(mulib_tls_session_ *)context;size_t n=0;int ok=0,request=0;
  unsigned char *certificates=NULL;mulib_public_key_t leaf;
  struct {unsigned char zero[32],empty[32],shared[32],early[32],derived[32],handshake[32],master[32],hash[32],rx[32],tx[32],finished[36];} k;
  memset(&k,0,sizeof k);*verified=0;
  if(!mulib_tls_hello_(s)||!mulib_tls_message_(s,0,&n)||!mulib_tls_serverhello_(s,n,k.shared)||s->hand_length!=n)goto done;
  mulib_tls_consume_(s,n,1);mulib_tls_hash_(s,k.hash);
  (void)mulib_sha(MULIB_SHA256,NULL,0,k.empty,32);
  if(!mulib_hkdf_extract(MULIB_SHA256,k.zero,32,k.zero,32,k.early,32)
      ||!mulib_hkdf_tls13(k.early,"derived",k.empty,32,k.derived,32)
      ||!mulib_hkdf_extract(MULIB_SHA256,k.derived,32,k.shared,32,k.handshake,32)
      ||!mulib_hkdf_tls13(k.handshake,"s hs traffic",k.hash,32,k.rx,32)
      ||!mulib_hkdf_tls13(k.handshake,"c hs traffic",k.hash,32,k.tx,32)
      ||!mulib_tls_keys_derive_(&s->rx,k.rx)||!mulib_tls_keys_derive_(&s->tx,k.tx))goto done;
  if(!mulib_tls_message_(s,1,&n)||!mulib_tls_extensions_(s,n))goto done;
  mulib_tls_consume_(s,n,1);
  if(!mulib_tls_message_(s,1,&n))goto done;
  if(s->hand[0]==13) {
    if(!mulib_tls_certrequest_(s,n))goto done;
    request=1;mulib_tls_consume_(s,n,1);if(!mulib_tls_message_(s,1,&n))goto done;
  }
  certificates=(unsigned char *)malloc(n);if(!certificates)goto done;
  memcpy(certificates,s->hand,n);
  if(!mulib_tls_certificates_(s,certificates,n,&leaf))goto done;
  mulib_tls_consume_(s,n,1);
  if(!mulib_tls_message_(s,1,&n)||!mulib_tls_certverify_(s,n,&leaf))goto done;
  mulib_tls_consume_(s,n,1);
  if(!mulib_tls_message_(s,1,&n)||n!=36||s->hand[0]!=20||s->hand_length!=n||!mulib_tls_finished_(s,k.rx,k.finished)
      ||!mulib_secret_equal(k.finished,s->hand+4,32))goto done;
  mulib_tls_consume_(s,n,1);mulib_tls_hash_(s,k.hash);
  if(!mulib_hkdf_tls13(k.handshake,"derived",k.empty,32,k.derived,32)
      ||!mulib_hkdf_extract(MULIB_SHA256,k.derived,32,k.zero,32,k.master,32)
      ||!mulib_hkdf_tls13(k.master,"s ap traffic",k.hash,32,k.rx,32)
      ||!mulib_hkdf_tls13(k.master,"c ap traffic",k.hash,32,k.early,32))goto done;
  if(request) {
    static const unsigned char empty_cert[]={11,0,0,4,0,0,0,0};
    if(!mulib_tls_send_(s,22,empty_cert,sizeof empty_cert,1))goto done;
    (void)mulib_sha_update(&s->transcript,empty_cert,sizeof empty_cert);
  }
  k.finished[0]=20;k.finished[1]=k.finished[2]=0;k.finished[3]=32;
  if(!mulib_tls_finished_(s,k.tx,k.finished+4)||!mulib_tls_send_(s,22,k.finished,36,1)
      ||!mulib_tls_keys_derive_(&s->rx,k.rx)||!mulib_tls_keys_derive_(&s->tx,k.early))goto done;
  s->record_length=s->record_offset=0;s->ready=1;*verified=1;ok=1;
done:
  free(certificates);mulib_secret_wipe(&k,sizeof k);mulib_secret_wipe(s->private_key,32);mulib_secret_wipe(&s->transcript,sizeof s->transcript);
  mulib_x509_store_free(&s->owned_trust);return ok?MULIB_TSL_OK:MULIB_TSL_EAUTH;
}
static int mulib_tls_update_ (mulib_tls_keys_ *key)
{
  unsigned char secret[32];int ok=mulib_hkdf_tls13(key->secret,"traffic upd",NULL,0,secret,32)&&mulib_tls_keys_derive_(key,secret);
  mulib_secret_wipe(secret,32);return ok;
}
static int mulib_tls_ticket_ (const unsigned char *p,size_t n)
{
  mulib_tls_cursor_ c={p,n},nonce,ticket,ext,v;const unsigned char *bytes;unsigned seen[64],count=0;
  if(!mulib_tls_take_(&c,8,&bytes)||!mulib_tls_vec_(&c,1,&nonce)||!mulib_tls_vec_(&c,2,&ticket)||!ticket.n||!mulib_tls_vec_(&c,2,&ext)||c.n)return 0;
  while(ext.n) {
    unsigned id;if(count==64||!mulib_tls_take_(&ext,2,&bytes)||!mulib_tls_vec_(&ext,2,&v))return 0;id=mulib_tls_u16_(bytes);
    for(unsigned i=0;i<count;++i)if(seen[i]==id)return 0;
    seen[count++]=id;if(id==42&&v.n!=4)return 0;
  }
  return 1;
}
static int mulib_tls_read_ (void *context,void *out,size_t capacity,size_t *count)
{
  mulib_tls_session_ *s=(mulib_tls_session_ *)context;*count=0;
  for(unsigned attempts=0;attempts<256;++attempts) {
    if(s->record_offset<s->record_length) {
      size_t n=s->record_length-s->record_offset;if(n>capacity)n=capacity;
      memcpy(out,s->record+s->record_offset,n);s->record_offset+=n;*count=n;return 0;
    }
    if(s->closed)return MULIB_TSL_EOF;
    {unsigned type;
      if(!mulib_tls_record_(s,1,&type))return MULIB_TSL_EBACKEND;
      if(type==23) {if(s->hand_length)return MULIB_TSL_EBACKEND;continue;}
      if(type==21) {s->record_length=0;continue;}
      if(s->record_length>sizeof s->hand-s->hand_length)return MULIB_TSL_EBACKEND;
      memcpy(s->hand+s->hand_length,s->record,s->record_length);s->hand_length+=s->record_length;s->record_length=0;
      while(s->hand_length>=4) {
        size_t n=mulib_tls_u24_(s->hand+1)+4;
        if(n>sizeof s->hand)return MULIB_TSL_EBACKEND;
        if(s->hand_length<n)break;
        if(s->hand[0]==24) {
          static const unsigned char update[]={24,0,0,1,0};
          if(n!=5||s->hand_length!=n||s->hand[4]>1||!mulib_tls_update_(&s->rx))return MULIB_TSL_EBACKEND;
          if(s->hand[4]&&(!mulib_tls_send_(s,22,update,5,1)||!mulib_tls_update_(&s->tx)))return MULIB_TSL_EBACKEND;
        } else if(s->hand[0]!=4||!mulib_tls_ticket_(s->hand+4,n-4))return MULIB_TSL_EBACKEND;
        mulib_tls_consume_(s,n,0);
      }
    }
  }
  return MULIB_TSL_EBACKEND;
}
static int mulib_tls_write_ (void *context,const void *data,size_t n,size_t *count)
{
  mulib_tls_session_ *s=(mulib_tls_session_ *)context;*count=0;if(n>16384)n=16384;
  if(s->closed||!mulib_tls_send_(s,23,data,n,1))return MULIB_TSL_EBACKEND;
  *count=n;return 0;
}
static void mulib_tls_close_ (void *context)
{
  mulib_tls_session_ *s=(mulib_tls_session_ *)context;
  if(s) {
    if(s->ready) {static const unsigned char alert[]={1,0};(void)mulib_tls_send_(s,21,alert,2,1);}
    (void)mulib_socket_close(&s->socket);mulib_x509_store_free(&s->owned_trust);mulib_secret_wipe(s,sizeof *s);free(s);
  }
}
int mulib_tls_connect (void *user,const mulib_uri_t *u,mulib_tsl_t *out)
{
  static const mulib_tsl_ops_t ops={mulib_tls_handshake_,mulib_tls_read_,mulib_tls_write_,mulib_tls_close_};
  const mulib_tls_options_t *options=(const mulib_tls_options_t *)user;mulib_tls_session_ *s;
  size_t n,start=0;unsigned port=443;
  if(!out||out->state||!u||!u->has_authority||u->has_userinfo||u->scheme.length!=5)return 0;
  for(unsigned i=0;i<5;++i) {unsigned ch=(unsigned char)u->scheme.data[i];if(ch>='A'&&ch<='Z')ch+=32;if(ch!=(unsigned char)"https"[i])return 0;}
  n=u->host.length;if(n>2&&u->host.data[0]=='['&&u->host.data[n-1]==']'){start=1;n-=2;}
  if(!n||n>253||memchr(u->host.data+start,0,n))return 0;
  if(u->has_port) {port=0;if(!u->port.length)return 0;for(size_t i=0;i<u->port.length;++i){unsigned d=(unsigned char)u->port.data[i]-'0';if(d>9||port>(65535u-d)/10)return 0;port=port*10+d;}if(!port)return 0;}
  s=(mulib_tls_session_ *)calloc(1,sizeof *s);if(!s)return 0;
  s->socket.fd=-1;s->timeout=options&&options->timeout_ms?options->timeout_ms:30000;
  memcpy(s->host,u->host.data+start,n);s->host[n]=0;
  if(options&&options->trust)s->trust=options->trust;
  else {
    if(!(options&&options->ca_file?mulib_x509_store_file(&s->owned_trust,options->ca_file):mulib_x509_store_system(&s->owned_trust)))goto fail;
    s->trust=&s->owned_trust;
  }
  if(!s->trust->count||mulib_socket_connect_host(&s->socket,s->host,port,s->timeout)||!mulib_tsl_init(out,&ops,s))goto fail;
  return 1;
fail:
  mulib_tls_close_(s);return 0;
}
#undef MULIB_TLS_HAND_MAX_
#endif
