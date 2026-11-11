#include "fixtures/muw3-setup.h"
#include <assert.h>
#include <stdio.h>
static void pair(mulib_tls_session_ **client,mulib_tls_session_ **server)
{
  unsigned char secret[32]={1};
  *client=(mulib_tls_session_ *)calloc(1,sizeof **client);
  *server=(mulib_tls_session_ *)calloc(1,sizeof **server);
  assert(*client&&*server);
  (*client)->socket.fd=(*server)->socket.fd=-1;
  assert(!mulib_socket_pair(&(*client)->socket,&(*server)->socket));
  (*client)->ready=(*server)->ready=1;
  (*client)->timeout=(*server)->timeout=100;
  assert(mulib_tls_keys_derive_(&(*client)->rx,secret)&&mulib_tls_keys_derive_(&(*server)->tx,secret));
  secret[0]=2;
  assert(mulib_tls_keys_derive_(&(*client)->tx,secret)&&mulib_tls_keys_derive_(&(*server)->rx,secret));
}
static void release(mulib_tls_session_ *a,mulib_tls_session_ *b)
{a->ready=b->ready=0;mulib_tls_close_(a);mulib_tls_close_(b);}
static void records(void)
{
  mulib_tls_session_ *a,*b;unsigned char output[32];size_t count;unsigned type;
  static const unsigned char update[]={24,0,0,1,1};
  static const unsigned char ticket[]={4,0,0,14,0,0,0,1,0,0,0,2,0,0,1,42,0,0};
  static const unsigned char alert[]={1,0};
  pair(&a,&b);
  /* A ticket fragmented across handshake records, followed by a requested
   * KeyUpdate and application data under the next traffic secret. */
  assert(mulib_tls_send_(b,22,ticket,3,1)&&mulib_tls_send_(b,22,ticket+3,sizeof ticket-3,1));
  assert(mulib_tls_send_(b,22,update,sizeof update,1)&&mulib_tls_update_(&b->tx));
  assert(mulib_tls_send_(b,23,"hello",5,1));
  assert(!mulib_tls_read_(a,output,2,&count)&&count==2&&!memcmp(output,"he",2));
  assert(!mulib_tls_read_(a,output,32,&count)&&count==3&&!memcmp(output,"llo",3));
  assert(mulib_tls_record_(b,1,&type)&&type==22&&b->record_length==5&&b->record[0]==24&&b->record[4]==0);
  assert(mulib_tls_update_(&b->rx));
  assert(!mulib_tls_write_(a,"ok",2,&count)&&count==2);
  assert(mulib_tls_record_(b,1,&type)&&type==23&&b->record_length==2&&!memcmp(b->record,"ok",2));
  assert(mulib_tls_send_(b,21,alert,2,1));
  assert(mulib_tls_read_(a,output,32,&count)==MULIB_TSL_EOF&&!count);
  assert(mulib_tls_read_(a,output,32,&count)==MULIB_TSL_EOF&&!count);
  release(a,b);
  /* Authentication failure must not modify output. */
  pair(&a,&b);b->tx.sequence=1;memset(output,0xa5,sizeof output);
  assert(mulib_tls_send_(b,23,"secret",6,1));
  assert(mulib_tls_read_(a,output,sizeof output,&count)<0&&!count&&output[0]==0xa5);
  release(a,b);
  /* Application bytes cannot interrupt a partially received handshake. */
  pair(&a,&b);
  assert(mulib_tls_send_(b,22,ticket,3,1)&&mulib_tls_send_(b,23,"bad",3,1));
  assert(mulib_tls_read_(a,output,sizeof output,&count)<0&&!count);
  release(a,b);
  /* Reject record-number exhaustion instead of reusing a nonce. */
  pair(&a,&b);a->tx.sequence=UINT64_C(8388608);
  assert(mulib_tls_write_(a,"bad",3,&count)<0&&!count);
  release(a,b);
  /* TLS 1.3 forbids unencrypted application records and CCS after Finished. */
  pair(&a,&b);
  assert(mulib_tls_send_(b,20,"\1",1,0));
  assert(mulib_tls_read_(a,output,sizeof output,&count)<0&&!count);
  release(a,b);
}
int main(void)
{
  records();puts("TLS: fragmented tickets, bidirectional KeyUpdate, record authentication and shutdown passed");return 0;
}
