#include "../tls-setup.h"
#include <stdio.h>
int main(int argc,char **argv)
{
  mulib_x509_store_t anchors={0},chain={0};int ok;
  if(argc!=4){puts("Usage: x509 ROOTS.pem CHAIN.pem HOST");return 0;}
  ok=mulib_x509_store_file(&anchors,argv[1])&&mulib_x509_store_file(&chain,argv[2])
      &&mulib_x509_verify(chain.certificates,chain.count,&anchors,argv[3],(int64_t)time(NULL));
  puts(ok?"Verified certificate chain and host":"Certificate verification failed");
  mulib_x509_store_free(&anchors);mulib_x509_store_free(&chain);return ok?0:1;
}
