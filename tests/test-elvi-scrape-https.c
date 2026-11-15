#include "fixtures/elvi-scrape-setup.h"
#include <assert.h>
#include <stdio.h>

typedef struct fixture {
  const char *wire;
  size_t offset;
  unsigned connects, closes, writes;
  int mode;
} fixture;
static int handshake(void *ctx,int *verified) {
  fixture *f=ctx;*verified=f->mode==1?0:1;return MULIB_TSL_OK;
}
static int read_tls(void *ctx,void *out,size_t cap,size_t *n) {
  fixture *f=ctx;size_t left=strlen(f->wire)-f->offset;
  *n=0;if(!left)return f->mode==8?MULIB_TSL_EBACKEND:MULIB_TSL_EOF;
  if(cap>7)cap=7;
  if(left>cap)left=cap;
  memcpy(out,f->wire+f->offset,left);f->offset+=left;*n=left;return MULIB_TSL_OK;
}
static int write_tls(void *ctx,const void *data,size_t n,size_t *written) {
  fixture *f=ctx;(void)data;f->writes++;*written=n>3?3:n;return MULIB_TSL_OK;
}
static void close_tls(void *ctx) {((fixture *)ctx)->closes++;}
static int connect_tls(void *ctx,const mulib_uri_t *endpoint,mulib_tsl_t *s) {
  fixture *f=ctx;mulib_tsl_ops_t ops={handshake,read_tls,write_tls,close_tls};
  assert(endpoint->host.length==11 && !memcmp(endpoint->host.data,"example.org",11));
  f->connects++;assert(mulib_tsl_init(s,&ops,ctx));return f->mode==7?0:1;
}
static int peer_tls(void *ctx,const mulib_tsl_t *s,mulib_elvi_scrape_peer_t *peer) {
  fixture *f=ctx;assert(s->state==2);
  strcpy(peer->hostname,f->mode==3?"evil.example":"example.org");
  peer->certificate_verified=f->mode==2?0:1;peer->hostname_verified=f->mode==9?0:1;
  peer->tls_version=f->mode==4?0x0302:0x0304;
  strcpy(peer->alpn,f->mode==5?"h2":"http/1.1");return f->mode==6?0:1;
}
int main(void) {
  const char *wire="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 2\r\n\r\nOK";
  for(int mode=0;mode<=9;mode++) {
    fixture f={wire,0,0,0,0,mode};
    mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
    mulib_shylock_buffer_t body={0};char type[64]="unchanged";unsigned status=42;
    int ok=mulib_elvi_scrape_https_transport(&tls,"https://example.org/",&body,&status,type,sizeof type);
    assert(f.connects==1 && f.closes==1);
    if(!mode)assert(ok && status==200 && body.len==2 && !strcmp(body.data,"OK"));
    else {assert(!ok && status==42 && !strcmp(type,"unchanged") && !body.data);
      if(mode!=8)assert(!f.writes);}
    mulib_shylock_buffer_free(&body);
  }
  /* A redirect response is returned for the search wrapper to reject. No
   * Location, including private/HTTP targets, causes a second connection. */
  const char *targets[]={"http://example.org/","https://127.0.0.1/","https://example.net/"};
  for(size_t i=0;i<3;i++) {
    char raw[256];snprintf(raw,sizeof raw,"HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n",targets[i]);
    fixture f={raw,0,0,0,0,0};mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
    mulib_shylock_buffer_t body={0};char type[64];unsigned status=0;
    assert(mulib_elvi_scrape_https_transport(&tls,"https://example.org/",&body,&status,type,sizeof type));
    assert(status==302 && f.connects==1 && f.closes==1);mulib_shylock_buffer_free(&body);
  }
  fixture f={wire,0,0,0,0,0};mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
  mulib_shylock_buffer_t body={0};char type[64];unsigned status=0;
  assert(!mulib_elvi_scrape_https_transport(&tls,"https://localhost/",&body,&status,type,sizeof type));
  assert(!mulib_elvi_scrape_https_transport(&tls,"http://example.org/",&body,&status,type,sizeof type));
  assert(!f.connects);body.limit=1;
  assert(!mulib_elvi_scrape_https_transport(&tls,"https://example.org/",&body,&status,type,sizeof type));
  assert(f.connects==1 && f.closes==1 && !body.data);
  puts("elvi-scrape HTTPS: OK");return 0;
}
