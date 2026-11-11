#include "tls-setup.h"
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_HTTPS
#define MULIB_IMPL_MUW3
#include "mulib-muw3.h"
#include <stdio.h>
int main(int argc,char **argv)
{
  mulib_muw3_client_t client=mulib_muw3_client_default();mulib_muw3_request_t request={0};mulib_muw3_response_t response={0};int rc;
  if(argc<2||argc>3){puts("Usage: muw3-https https://HOST/PATH [CA.pem]");return 0;}
  request.url=argv[1];if(argc==3)client.options.ca_file=argv[2];
  rc=mulib_muw3_perform(&client,&request,&response);
  if(rc!=MULIB_MUW3_OK){fprintf(stderr,"%s\n",mulib_muw3_strerror(rc));return 1;}
  rc=fwrite(response.message.body,1,response.message.body_length,stdout)==response.message.body_length?0:1;
  mulib_muw3_response_free(&response);return rc;
}
