/* Example: build a provider URL and optionally parse a saved response. No network.
 * Build: cc -std=c11 -Iinclude examples/elvi-scrape.c -lm -o demo
 * Usage: ./demo library/elvis/Google.elvs "C11 compiler" [response.html|json]
 */
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_BOUNDED_TEXT
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_FETCH_POLICY
#define MULIB_IMPL_FETCH
#define MULIB_IMPL_URL
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_TSL
#define MULIB_IMPL_HTTPS
#define MULIB_IMPL_ELVI_SCRAPE
#include "mulib-elvi-scrape.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
  mulib_elvi_scrape_program_t p={0};mulib_elvi_scrape_error_t e={0};
  mulib_elvi_scrape_result_t results[20];char url[8192];
  char *body;long n;FILE *f;size_t count=0;
  if(argc!=3 && argc!=4){fprintf(stderr,"usage: %s provider.elvs query [saved-response]\n",argv[0]);return 2;}
  if(!mulib_elvi_scrape_load(argv[1],&p,&e)){
    fprintf(stderr,"provider line %zu: %s\n",e.line,e.message);return 1;
  }
  unsigned limit=p.max_results<20?p.max_results:20;
  if(!mulib_elvi_scrape_url(&p,argv[2],limit,url,sizeof url)){
    fprintf(stderr,"could not construct request URL\n");return 1;
  }
  printf("request: %s\n",url);
  if(argc==3)return 0;
  f=fopen(argv[3],"rb");if(!f){perror(argv[3]);return 1;}
  if(fseek(f,0,SEEK_END)!=0 || (n=ftell(f))<0 || n>2097152 ||
     fseek(f,0,SEEK_SET)!=0){fclose(f);return 1;}
  body=malloc((size_t)n+1);
  if(!body){fclose(f);return 1;}
  if(fread(body,1,(size_t)n,f)!=(size_t)n || ferror(f)){free(body);fclose(f);return 1;}
  fclose(f);
  if(!mulib_elvi_scrape_parse(&p,body,(size_t)n,results,limit,&count)){
    free(body);fprintf(stderr,"response parse failed\n");return 1;
  }
  for(size_t i=0;i<count;i++) printf("%zu. %s\n   %s\n",i+1,results[i].title,results[i].url);
  free(body);return 0;
}
