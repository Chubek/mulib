#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MULIB_IMPL_WEBSOCKET
#define MULIB_IMPL_GRPC
#define MULIB_IMPL_SOAP
#include "mulib-websocket.h"
#include "mulib-grpc.h"
#include "mulib-soap.h"

/* Isolated stand-ins for existing MuLIB providers: keep functional checks
   fully offline. Actual Mulib integration is a separate validation step. */
int mulib_buffer_append(mulib_buffer_t *b,const void *p,size_t n) {
  size_t cap=b->length+n;unsigned char *q;
  if(cap<b->length)return 0;
  q=(unsigned char*)realloc(b->data,cap?cap:1);
  if(!q)return 0;
  b->data=q;memcpy(q+b->length,p,n);b->length=cap;b->capacity=cap;return 1;
}
void mulib_buffer_free(mulib_buffer_t *b){free(b->data);memset(b,0,sizeof(*b));}
static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
size_t mulib_base64_encode(const void *p,size_t n,char *out,size_t cap) {
  const unsigned char *s=(const unsigned char *)p;size_t written=((n+2)/3)*4,pos=0;
  if(cap<written+1)return 0;
  for(size_t i=0;i<n;i+=3){
    unsigned v=(unsigned)s[i]<<16;
    if(i+1<n)v|=(unsigned)s[i+1]<<8;
    if(i+2<n)v|=s[i+2];
    out[pos++]=b64[(v>>18)&63];out[pos++]=b64[(v>>12)&63];
    out[pos++]=i+1<n?b64[(v>>6)&63]:'=';
    out[pos++]=i+2<n?b64[v&63]:'=';
  }
  out[written]=0;return written;
}
int mulib_base64_decode(const char *s,size_t n,unsigned char *out,size_t cap,size_t *written) {
  size_t z=0;if(!s||n%4)return 0;
  for(size_t i=0;i<n;i+=4){
    unsigned v=0;int cnt=0;
    for(int j=0;j<4;j++){
      int k=0;char c=s[i+j];
      if(c=='='){if(j<2 || i+4!=n)return 0;cnt++;k=0;}
      else{if(cnt)return 0;while(k<64 && b64[k]!=c)k++;if(k==64)return 0;}
      v=(v<<6)|(unsigned)k;
    }
    if(cap-z<(size_t)(3-cnt))return 0;
    out[z++]=(unsigned char)(v>>16);
    if(cnt<2)out[z++]=(unsigned char)(v>>8);
    if(cnt<1)out[z++]=(unsigned char)v;
  }
  if(written)*written=z;
  return 1;
}
const char *mulib_http_header(const mulib_http_message_t *r,const char *name) {
  if(!r)return NULL;
  for(size_t i=0;i<r->header_count;i++)if(!strcmp(r->headers[i].name,name))return r->headers[i].value;
  return NULL;
}
static const unsigned char grpc_reply[]={0,0,0,0,2,'O','K',0x80,0,0,0,16,
  'g','r','p','c','-','s','t','a','t','u','s',':',' ','0','\r','\n'};
int mulib_muw3_perform(const mulib_muw3_client_t *client,const mulib_muw3_request_t *req,
    mulib_muw3_response_t *out) {
  static mulib_http_header_t h[]={{"Content-Type","application/grpc-web+proto"}};
  (void)client;
  assert(!strcmp(req->method,"POST"));
  assert(req->body_length==8);assert(!memcmp(req->body,"\0\0\0\0\3HEY",8));
  out->message.is_response=1;out->message.status=200;
  out->message.headers=h;out->message.header_count=1;
  out->message.body=(unsigned char*)malloc(sizeof(grpc_reply));
  assert(out->message.body);
  memcpy(out->message.body,grpc_reply,sizeof(grpc_reply));
  out->message.body_length=sizeof(grpc_reply);
  return 1;
}
void mulib_muw3_response_free(mulib_muw3_response_t *r) {
  if(r){free(r->message.body);memset(r,0,sizeof(*r));}
}
int main(void) {
  const char *sample="dGhlIHNhbXBsZSBub25jZQ==";
  const char *expected="s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
  char accept[29],key[25];unsigned char rand16[16];
  mulib_buffer_t buffer={0};mulib_websocket_frame_t frame={0};
  mulib_grpc_frame_t gframe={0};size_t used=999;
  const unsigned char mask[4]={1,2,3,4};
  char *xml,*escaped,*upgrade;
  mulib_grpc_web_reply_t reply={0};
  assert(mulib_websocket_accept(sample,accept) && !strcmp(expected,accept));
  assert(!mulib_websocket_accept("garbage",accept));
  for(int i=0;i<16;i++)rand16[i]=(unsigned char)i;
  assert(mulib_websocket_key(rand16,key));
  upgrade=mulib_websocket_upgrade_request("localhost:8080","/ws?x=1",key,"chat");
  assert(upgrade && strstr(upgrade,"Upgrade: websocket\r\n"));free(upgrade);
  assert(!mulib_websocket_upgrade_request("hi\r\nInjected: 1","/",key,NULL));
  assert(mulib_websocket_frame_write(&buffer,2,1,1,mask,"A\0B",3));
  assert(mulib_websocket_frame_read(buffer.data,buffer.length,10,1,&frame,&used)==1);
  assert(used==buffer.length && frame.opcode==2 && frame.masked==1);
  assert(frame.length==3 && !memcmp(frame.payload,"A\0B",3));
  mulib_websocket_frame_free(&frame);
  assert(mulib_websocket_frame_read(buffer.data,buffer.length-1,10,1,&frame,&used)==0);
  assert(mulib_websocket_frame_read(buffer.data,buffer.length,10,0,&frame,&used)==-1);
  mulib_buffer_free(&buffer);
  assert(mulib_grpc_frame_write(&buffer,0,"HELLO",5));
  assert(mulib_grpc_frame_read(buffer.data,buffer.length,1024,&gframe,&used)==1);
  assert(used==10 && gframe.length==5 && !memcmp(gframe.data,"HELLO",5));
  assert(mulib_grpc_frame_read(buffer.data,9,1024,&gframe,&used)==0);
  mulib_buffer_free(&buffer);
  escaped=mulib_soap_escape_text("a<&>'\"",6,100);
  assert(escaped && !strcmp(escaped,"a&lt;&amp;&gt;&apos;&quot;"));free(escaped);
  xml=mulib_soap_envelope(0,"<auth/>","<x>hi</x>",1024);
  assert(xml && strstr(xml,"<soap:Header><auth/></soap:Header>") &&
      strstr(xml,"<soap:Body><x>hi</x></soap:Body>"));free(xml);
  assert(!mulib_soap_escape_text("\1",1,1024));
  assert(mulib_grpc_web_unary(NULL,"http://test/grpc","HEY",3,1024,&reply)==1);
  assert(reply.grpc_status==0 && reply.message_length==2 && !memcmp(reply.message,"OK",2));
  mulib_grpc_web_reply_free(&reply);
  puts("PASS: WebSocket RFC6455 handshake/framing, gRPC framing/gRPC-Web, SOAP envelope/escaping");
  return 0;
}
