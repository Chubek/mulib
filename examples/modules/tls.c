#include "../tls-setup.h"
#include <stdio.h>
int main(int argc,char **argv)
{
  mulib_uri_t endpoint;mulib_tsl_t session={0};mulib_tls_options_t options={0};
  char request[512];unsigned char response[4096];int n,rc=1;size_t offset=0;
  if(argc<2||argc>3){puts("Usage: tls https://HOST[:PORT]/ [CA.pem] (requests /)");return 0;}
  if(argc==3)options.ca_file=argv[2];
  if(!mulib_uri_parse(argv[1],strlen(argv[1]),&endpoint)||endpoint.host.length>253)return 1;
  n=snprintf(request,sizeof request,"GET / HTTP/1.1\r\nHost: %.*s\r\nConnection: close\r\n\r\n",(int)endpoint.host.length,endpoint.host.data);
  if(n<0||(size_t)n>=sizeof request||!mulib_tls_connect(&options,&endpoint,&session)||mulib_tsl_handshake(&session))goto done;
  while(offset<(size_t)n){size_t count;if(mulib_tsl_write(&session,request+offset,(size_t)n-offset,&count))goto done;offset+=count;}
  for(;;){size_t count;int status=mulib_tsl_read(&session,response,sizeof response,&count);if(status==MULIB_TSL_EOF){rc=0;break;}if(status||fwrite(response,1,count,stdout)!=count)break;}
done:
  mulib_tsl_close(&session);return rc;
}
