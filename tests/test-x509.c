#include "fixtures/tls-setup.h"
#include <assert.h>
#include <time.h>
int main(int argc,char **argv)
{
  mulib_x509_store_t roots={0},chain={0};
  if(argc==1) {
    unsigned char bad[]={0x30,0};mulib_x509_certificate_t cert;
    assert(!mulib_x509_parse(bad,sizeof bad,&cert));
    puts("X.509 malformed certificate rejected");return 0;
  }
  if(argc!=4)return 2;
  if(!mulib_x509_store_file(&roots,argv[1])||!mulib_x509_store_file(&chain,argv[2]))return 3;
  {
    int ok=mulib_x509_verify(chain.certificates,chain.count,&roots,argv[3],(int64_t)time(NULL));
    mulib_x509_store_free(&roots);mulib_x509_store_free(&chain);
    return ok?0:1;
  }
}
