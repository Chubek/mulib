#include "../examples/mumq-common.h"
#include <stdlib.h>
static void crypto(void) {
  unsigned char key[64]={0}, nonce[12]={0}, in[32]={0}, out[32], recovered[32], tag[32];
  const unsigned char block[16]={0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
  const unsigned char expected[16]={0x8e,0xa2,0xb7,0xca,0x51,0x67,0x45,0xbf,0xea,0xfc,0x49,0x90,0x4b,0x49,0x60,0x89};
  const unsigned char ctr[16]={0x53,0x0f,0x8a,0xfb,0xc7,0x45,0x36,0xb9,0xa9,0x63,0xb4,0xf1,0xc4,0xcb,0x73,0x8b};
  for (unsigned i=0;i<32;++i) key[i]=(unsigned char)i;
  assert(mulib_aes256_block(key,block,out) && !memcmp(out,expected,16));
  memset(key,0,64);
  assert(mulib_aes256_ctr(key,nonce,in,16,out) && !memcmp(out,ctr,16));
  for (size_t n=0;n<=32;++n) {
    assert(mulib_aes_seal(key,nonce,"aad",3,in,n,out,tag));
    assert(mulib_aes_open(key,nonce,"aad",3,out,n,tag,recovered) && !memcmp(in,recovered,n));
    memset(recovered,0x5a,32); tag[0]^=1;
    assert(!mulib_aes_open(key,nonce,"aad",3,out,n,tag,recovered));
    assert(recovered[0]==0x5a); tag[0]^=1;
    assert(!mulib_aes_open(key,nonce,"bad",3,out,n,tag,recovered));
  }
  assert(mulib_aes256_ctr(key,nonce,in,32,in));
  assert(mulib_aes256_ctr(key,nonce,in,32,in));
  for (unsigned i=0;i<32;++i) assert(!in[i]);
  assert(!mulib_aes_seal(NULL,nonce,NULL,0,in,0,out,tag));
  assert(!mulib_aes_seal(key,nonce,NULL,1,in,1,out,tag));
}
static void options(mulib_mumq_options_t *a, mulib_mumq_options_t *b, unsigned char key[64]) {
  memset(a,0,sizeof *a); memset(b,0,sizeof *b);
  a->key=b->key=key; a->tx_prefix[0]=b->rx_prefix[0]=1; a->rx_prefix[0]=b->tx_prefix[0]=2;
}
static void secure_frames(void) {
  mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  mulib_mumq_options_t ao,bo; unsigned char key[64]={0};
  mulib_mumq_t *tx,*rx; mulib_mumq_message_t m={0};
  options(&ao,&bo,key);
  assert(!mulib_socket_pair(&a,&b)); tx=mulib_mumq_create(&a,&ao); rx=mulib_mumq_create(&b,&bo);
  assert(tx&&rx&&a.fd==-1&&b.fd==-1);
  assert(!mulib_mumq_send(tx,1,"a/b","\0\1\2",3));
  assert(!mulib_mumq_flush(tx));
  assert(!mulib_mumq_receive(rx,&m) && m.length==3 && !memcmp(m.data,"\0\1\2",3));
  mulib_mumq_message_free(&m);
  assert(!mulib_mumq_send(tx,1,"a/b","secret",6));
  tx->first->bytes[35]^=1; /* Ciphertext, after encrypted topic. */
  assert(!mulib_mumq_flush(tx)); assert(mulib_mumq_receive(rx,&m)==MULIB_MUMQ_AUTH);
  assert(!m.data); mulib_mumq_destroy(tx); mulib_mumq_destroy(rx);
  assert(!mulib_socket_pair(&a,&b)); tx=mulib_mumq_create(&a,NULL); rx=mulib_mumq_create(&b,&bo);
  assert(tx&&rx&&!mulib_mumq_send(tx,1,"x",NULL,0)&&!mulib_mumq_flush(tx));
  assert(mulib_mumq_receive(rx,&m)==MULIB_MUMQ_PROTOCOL);
  mulib_mumq_destroy(tx); mulib_mumq_destroy(rx);
}
static void fragment_and_replay(void) {
  mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  mulib_mumq_t *tx,*rx; mulib_mumq_message_t m={0}; size_t sent, size;
  unsigned char frame[512];
  assert(!mulib_socket_pair(&a,&b)); tx=mulib_mumq_create(&a,NULL); rx=mulib_mumq_create(&b,NULL);
  assert(tx&&rx&&!mulib_mumq_send(tx,1,"topic","message",7));
  size=tx->first->size; assert(size<sizeof frame); memcpy(frame,tx->first->bytes,size);
  for (size_t i=0;i<size;++i) {
    assert(!mulib_socket_send(&tx->socket,frame+i,1,&sent)&&sent==1);
    assert(mulib_mumq_receive(rx,&m)==(i+1==size?0:1));
  }
  assert(m.data&&m.length==7); mulib_mumq_message_free(&m);
  assert(!mulib_socket_send(&tx->socket,frame,size,&sent)&&sent==size);
  assert(mulib_mumq_receive(rx,&m)==MULIB_MUMQ_PROTOCOL);
  mulib_mumq_destroy(tx); mulib_mumq_destroy(rx);
}
static int deny(void *u, size_t peer, unsigned type, const char *topic) {
  (void)u; (void)peer; (void)type; (void)topic; return 0;
}
static void bounds_and_acl(void) {
  mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  mulib_mumq_t *tx,*rx; size_t peer;
  mulib_mumq_server_t *s=mulib_mumq_server_create(deny,NULL);
  assert(s&&!mulib_socket_pair(&a,&b)); tx=mulib_mumq_create(&a,NULL); rx=mulib_mumq_create(&b,NULL);
  assert(tx&&rx&&!mulib_mumq_server_add(s,rx,&peer));
  assert(mulib_mumq_server_add(s,rx,&peer)==-1);
  assert(!mulib_mumq_topic("")&&!mulib_mumq_topic("a/#")&&!mulib_mumq_topic("a b"));
  assert(mulib_mumq_send(tx,2,"x","a",1)==-1);
  assert(mulib_mumq_send(tx,1,"x","a",MULIB_MUMQ_MAX_PAYLOAD+1)==-1);
  for (unsigned i=0;i<16;++i) assert(!mulib_mumq_send(tx,1,"x",NULL,0));
  assert(mulib_mumq_send(tx,1,"x",NULL,0)==1);
  assert(!mulib_mumq_flush(tx)); assert(mulib_mumq_server_step(s)==MULIB_MUMQ_AUTH);
  assert(mulib_mumq_server_remove(s,peer)==-1);
  mulib_mumq_destroy(tx); mulib_mumq_server_destroy(s);
}
static void tcp(void) {
  mulib_socket_t listener=MULIB_SOCKET_INIT,a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  unsigned port=0;
  assert(!mulib_socket_listen(&listener,"127.0.0.1",0,&port)&&port);
  assert(mulib_socket_accept(&listener,&b)==1);
  assert(!mulib_socket_connect(&a,"127.0.0.1",port));
  assert(!mulib_socket_accept(&listener,&b));
  assert(!mulib_socket_close(&a)&&!mulib_socket_close(&b)&&!mulib_socket_close(&listener));
  assert(mulib_socket_connect(&a,"not-an-address",1)==-1);
}
static void malformed(void) {
  for (unsigned test=0;test<10;++test) {
    mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
    mulib_mumq_t *tx,*rx; mulib_mumq_message_t m={0}; unsigned char *h;
    assert(!mulib_socket_pair(&a,&b)); tx=mulib_mumq_create(&a,NULL); rx=mulib_mumq_create(&b,NULL);
    assert(tx&&rx&&!mulib_mumq_send(tx,1,"x",NULL,0)); h=tx->first->bytes;
    switch (test) {
    case 0: h[0]='?'; break;
    case 1: h[4]=4; break;
    case 2: h[5]=2; break;
    case 3: h[7]=0; break;
    case 4: h[6]=1; break;
    case 5: h[8]=1; break;
    case 6: h[19]=0; break;
    case 7: h[20]=1; break;
    case 8: h[32]=0; break;
    default: h[32]='#'; break;
    }
    assert(!mulib_mumq_flush(tx)); assert(mulib_mumq_receive(rx,&m)==MULIB_MUMQ_PROTOCOL);
    assert(!m.data); mulib_mumq_destroy(tx); mulib_mumq_destroy(rx);
  }
}
static void maximum(void) {
  mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  mulib_mumq_t *tx,*rx; mulib_mumq_message_t m={0}; char topic[256];
  unsigned char *data=(unsigned char *)malloc(MULIB_MUMQ_MAX_PAYLOAD);
  assert(data); memset(data,0xa5,MULIB_MUMQ_MAX_PAYLOAD); memset(topic,'a',255); topic[255]=0;
  assert(!mulib_socket_pair(&a,&b)); tx=mulib_mumq_create(&a,NULL); rx=mulib_mumq_create(&b,NULL);
  assert(tx&&rx&&!mulib_mumq_send(tx,1,topic,data,MULIB_MUMQ_MAX_PAYLOAD));
  for (unsigned i=0;i<10000&&!m.data;++i) {
    assert(mulib_mumq_flush(tx)>=0); assert(mulib_mumq_receive(rx,&m)>=0);
  }
  assert(m.length==MULIB_MUMQ_MAX_PAYLOAD&&!memcmp(m.data,data,m.length));
  mulib_mumq_message_free(&m);
  for (unsigned i=0;i<3;++i) assert(!mulib_mumq_send(tx,1,topic,data,MULIB_MUMQ_MAX_PAYLOAD));
  assert(mulib_mumq_send(tx,1,topic,data,MULIB_MUMQ_MAX_PAYLOAD)==MULIB_MUMQ_AGAIN);
  mulib_mumq_destroy(tx); mulib_mumq_destroy(rx);
  {
    mulib_mumq_options_t ao,bo; unsigned char key[64]={0};
    options(&ao,&bo,key);
    assert(!mulib_socket_pair(&a,&b)); tx=mulib_mumq_create(&a,&ao); rx=mulib_mumq_create(&b,&bo);
    assert(tx&&rx&&!mulib_mumq_send(tx,1,topic,data,MULIB_MUMQ_MAX_PAYLOAD));
    for (unsigned i=0;i<10000&&!m.data;++i) {
      assert(mulib_mumq_flush(tx)>=0); assert(mulib_mumq_receive(rx,&m)>=0);
    }
    assert(m.length==MULIB_MUMQ_MAX_PAYLOAD&&!memcmp(m.data,data,m.length));
    mulib_mumq_message_free(&m); mulib_mumq_destroy(tx); mulib_mumq_destroy(rx);
  }
  free(data);
}
static void backpressure(void) {
  mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  mulib_mumq_t *pub,*sub,*peer; size_t pubslot,subslot;
  mulib_mumq_message_t m={0}; unsigned sent=0,received=0; unsigned char payload[8192]={0};
  mulib_mumq_server_t *s=mulib_mumq_server_create(NULL,NULL);
  assert(s&&!mulib_socket_pair(&a,&b)); pub=mulib_mumq_create(&a,NULL); peer=mulib_mumq_create(&b,NULL);
  assert(pub&&peer&&!mulib_mumq_server_add(s,peer,&pubslot));
  assert(!mulib_socket_pair(&a,&b)); sub=mulib_mumq_create(&a,NULL); peer=mulib_mumq_create(&b,NULL);
  assert(sub&&peer&&!mulib_mumq_server_add(s,peer,&subslot));
  assert(!mulib_mumq_subscribe(sub,"topic"));
  for (unsigned i=0;i<100&&!m.data;++i) {
    assert(mulib_mumq_flush(sub)>=0); assert(!mulib_mumq_server_step(s));
    assert(mulib_mumq_receive(sub,&m)>=0);
  }
  assert(mulib_mumq_subscribed(&m,"topic")); mulib_mumq_message_free(&m);
  /* Saturate the subscriber's kernel and bounded application queues. */
  for (unsigned i=0;i<2000;++i) {
    if (sent<100) {
      payload[0]=(unsigned char)sent;
      if (!mulib_mumq_publish(pub,"topic",payload,sizeof payload)) ++sent;
    }
    assert(mulib_mumq_flush(pub)>=0); assert(!mulib_mumq_server_step(s));
  }
  assert(s->peers[pubslot].pending.data && s->peers[subslot].channel->frames==16);
  for (unsigned i=0;i<10000&&received<100;++i) {
    if (sent<100) {
      payload[0]=(unsigned char)sent;
      if (!mulib_mumq_publish(pub,"topic",payload,sizeof payload)) ++sent;
    }
    assert(mulib_mumq_flush(pub)>=0); assert(!mulib_mumq_server_step(s));
    if (!mulib_mumq_receive(sub,&m)) {
      assert(m.length==sizeof payload&&m.data[0]==received); ++received; mulib_mumq_message_free(&m);
    }
  }
  assert(sent==100&&received==100);
  mulib_mumq_destroy(pub); mulib_mumq_destroy(sub); mulib_mumq_server_destroy(s);
}
int main(void) {
  crypto(); secure_frames(); fragment_and_replay(); bounds_and_acl(); tcp(); malformed();
  maximum(); backpressure(); example_mumq(); return 0;
}
