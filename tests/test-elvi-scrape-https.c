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
  fixture *f=ctx;*verified=f->mode==1?0:1;
  return f->mode==13?MULIB_TSL_AGAIN:f->mode==14?MULIB_TSL_EBACKEND:MULIB_TSL_OK;
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
  peer->tls_version=f->mode==4?0x0302:f->mode==12?0x7f1c:f->mode==15?0x0303:0x0304;
  strcpy(peer->alpn,f->mode==5?"h2":f->mode==15?"":"http/1.1");
  if(f->mode==10)memset(peer->hostname,'x',sizeof peer->hostname);
  if(f->mode==11)memset(peer->alpn,'x',sizeof peer->alpn);
  return f->mode==6?0:1;
}
static void json_dsl_validation(void) {
  const char prefix[]="elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/?q={query}\n";
  const struct {const char *tail;size_t line;} invalid[]={
    {"format xml\nanchor any\n",5},
    {"anchor any\nresults /items\n",6},
    {"format json\nresults /items\ntitle /name\n",7},
    {"format json\nresults /items\ntitle /name\nurl /url~\n",8},
    {"format json\nresults /items\ntitle /na~2me\nurl /url\n",7},
    {"format json\nresults items\ntitle /name\nurl /url\n",6},
    {"format json\nresults /items\ntitle /name\nurl /url\nanchor any\n",9},
    {"format json\nresults /items\ntitle /name\nurl /url\nanchor_class hit\n",9},
    {"format json\nresults /items\ntitle /name\nurl /url\nrewrite google\n",9},
    {"format json\nresults /items\ntitle /name\nurl /url\nresults /\n",9}
  };
  mulib_elvi_scrape_program_t p;
  mulib_elvi_scrape_error_t err;
  unsigned char before[sizeof p];
  memset(&p,0xa5,sizeof p);memcpy(before,&p,sizeof p);
  for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++) {
    char dsl[512];
    int n=snprintf(dsl,sizeof dsl,"%s%s",prefix,invalid[i].tail);
    assert(n>0 && (size_t)n<sizeof dsl);
    assert(!mulib_elvi_scrape_compile(dsl,(size_t)n,&p,&err));
    assert(err.line==invalid[i].line && err.message[0] && !memcmp(&p,before,sizeof p));
  }
}
static void json_search(void) {
  const char dsl[]="elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/?q={query}\nformat json\nresults /items\n"
    "title /meta~1data/0/name\nurl /~0uri\nrewrite relative\n";
  const char *types[]={"Application/JSON; charset=utf-8","text/html",
    "application/jsonx","application/json junk","application/problem+json"};
  const char *payload="{\"items\":["
    "{\"meta/data\":[{\"name\":\"ignored\"}],\"~uri\":\"//example.net/\"},"
    "{\"meta/data\":[{\"name\":\"ignored\\u0000x\"}],\"~uri\":\"/nul-title\"},"
    "{\"meta/data\":[{\"name\":\"ignored\"}],\"~uri\":\"/nul\\u0000-url\"},"
    "{\"meta/data\":[{\"name\":\"  one \\uD83D\\uDE00 \\n &amp;  \"}],\"~uri\":\"/result\"},"
    "{\"meta/data\":[{\"name\":\"duplicate\"}],\"~uri\":\"https://example.org/result\"},"
    "{\"meta/data\":[{\"name\":\"two\"}],\"~uri\":\"https://example.net/two?x=&amp;\"}]}";
  mulib_elvi_scrape_program_t p;
  assert(mulib_elvi_scrape_compile(dsl,sizeof dsl-1,&p,NULL));
  for(size_t i=0;i<sizeof types/sizeof *types;i++) {
    char wire[1024];
    fixture f={wire,0,0,0,0,0};mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
    mulib_fetch_client_t client={{8192,2097152,0,1,0},mulib_elvi_scrape_https_transport,&tls};
    mulib_elvi_scrape_result_t results[4],before[4];size_t n=99;unsigned status=0;
    int written=snprintf(wire,sizeof wire,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
        "Content-Length: %zu\r\n\r\n%s",types[i],strlen(payload),payload);
    assert(written>0 && (size_t)written<sizeof wire);
    memset(results,0xa5,sizeof results);memcpy(before,results,sizeof results);
    assert(mulib_elvi_scrape_search(&p,&client,"test",2,results,4,&n,&status)==(i==0));
    assert(f.connects==1 && f.closes==1 && status==200);
    if(i)assert(n==99 && !memcmp(before,results,sizeof results));
    else {
      assert(n==2 && !strcmp(results[0].title,"one \xf0\x9f\x98\x80 &amp;"));
      assert(!strcmp(results[0].url,"https://example.org/result"));
      assert(!strcmp(results[1].url,"https://example.net/two?x=&amp;"));
      assert(!memcmp(before+2,results+2,2*sizeof *results));
    }
  }
  const char *bad[]={"null","{}","{\"items\":null}","{\"items\":[{}]} null","{\"items\":[],"};
  for(size_t i=0;i<sizeof bad/sizeof *bad;i++) {
    size_t n=99;
    assert(!mulib_elvi_scrape_parse(&p,bad[i],strlen(bad[i]),NULL,0,&n) && n==99);
  }
  const char empty[]="{\"items\":[]}";
  size_t n=99;
  assert(mulib_elvi_scrape_parse(&p,empty,sizeof empty-1,NULL,0,&n) && !n);
}
static void responses(void) {
  const char *invalid[]={
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK",
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: gzip\r\nContent-Length: 2\r\n\r\nOK",
    "HTTP/1.1 200 OK\r\nContent-Encoding: identity\r\nContent-Encoding: identity\r\nContent-Length: 2\r\n\r\nOK",
    "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK",
    "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 3\r\n\r\nOK",
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nO",
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 2\r\n\r\nOKextra"
  };
  for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++) {
    fixture f={invalid[i],0,0,0,0,0};mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
    mulib_shylock_buffer_t body={0};char type[64]="unchanged";unsigned status=42;
    assert(mulib_shylock_text(&body,"seed"));char *original=body.data;
    assert(!mulib_elvi_scrape_https_transport(&tls,"https://example.org/",&body,&status,type,sizeof type));
    assert(f.connects==1 && f.closes==1 && status==42 && !strcmp(type,"unchanged"));
    assert(body.data==original && body.len==4 && !strcmp(body.data,"seed"));
    mulib_shylock_buffer_free(&body);
  }
  const char *valid[]={
    "HTTP/1.1 200 OK\r\nContent-Type: Text/HTML; charset=utf-8\r\nContent-Encoding: Identity\r\nContent-Length: 2\r\n\r\nOK",
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nO\r\n1\r\nK\r\n0\r\n\r\n",
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nOK"
  };
  for(size_t i=0;i<sizeof valid/sizeof *valid;i++) {
    fixture f={valid[i],0,0,0,0,15};mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
    mulib_shylock_buffer_t body={0};char type[64];unsigned status=0;
    assert(mulib_shylock_text(&body,"seed"));body.limit=6;
    assert(mulib_elvi_scrape_https_transport(&tls,"https://example.org/",&body,&status,type,sizeof type));
    assert(f.connects==1 && f.closes==1 && status==200 && !strcmp(body.data,"seedOK"));
    mulib_shylock_buffer_free(&body);
  }
}
int main(void) {
  const char *wire="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 2\r\n\r\nOK";
  for(int mode=0;mode<=15;mode++) {
    fixture f={wire,0,0,0,0,mode};
    mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
    mulib_shylock_buffer_t body={0};char type[64]="unchanged";unsigned status=42;
    int ok=mulib_elvi_scrape_https_transport(&tls,"https://example.org/",&body,&status,type,sizeof type);
    assert(f.connects==1 && f.closes==1);
    if(!mode || mode==15)assert(ok && status==200 && body.len==2 && !strcmp(body.data,"OK"));
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
  responses();
  json_dsl_validation();
  json_search();
  puts("elvi-scrape HTTPS: OK");return 0;
}
