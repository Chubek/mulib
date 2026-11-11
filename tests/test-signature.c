#include "fixtures/tls-setup.h"
#include <assert.h>
int main(int argc,char **argv)
{
  mulib_x509_store_t cert={0};unsigned char signature[512];size_t n;FILE *f;unsigned scheme;int ok;
  if(argc==1) {
    mulib_public_key_t bad={0};
    assert(!mulib_signature_verify(&bad,0x0804,"message",7,"bad",3));
    puts("Signature invalid-key rejection passed");return 0;
  }
  if(argc!=4||!mulib_x509_store_file(&cert,argv[1]))return 2;
  f=fopen(argv[2],"rb");if(!f)return 2;n=fread(signature,1,sizeof signature,f);fclose(f);
  scheme=(unsigned)strtoul(argv[3],NULL,16);
  ok=mulib_signature_verify(&cert.certificates[0].public_key,scheme,"MuLIB signature oracle",22,signature,n);
  if(ok) {
    signature[n/2]^=1;
    assert(!mulib_signature_verify(&cert.certificates[0].public_key,scheme,"MuLIB signature oracle",22,signature,n));
    signature[n/2]^=1;
    assert(!mulib_signature_verify(&cert.certificates[0].public_key,scheme,"MuLIB signature oraclf",22,signature,n));
  }
  mulib_x509_store_free(&cert);return ok?0:1;
}
