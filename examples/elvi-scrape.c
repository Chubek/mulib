/* Example: build provider URL and parse a saved HTML page (no network).
 * Requires linking with a translation unit implementing this module and
 * the MuLIB URL/FETCH dependencies. See manual/mulib-elvi-scrape.md.
 * Usage: ./demo library/elvi-scrape/google.elvs "C11 compiler" search.html
 */
#include "mulib-elvi-scrape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
  mulib_elvi_scrape_program_t p={0};mulib_elvi_scrape_error_t e={0};
  mulib_elvi_scrape_result_t results[20];char url[8192];
  unsigned char *html=NULL;long n;FILE *f;size_t count=0;
  if(argc!=4){fprintf(stderr,"usage: %s provider.elvs query saved.html\n",argv[0]);return 2;}
  if(!mulib_elvi_scrape_load(argv[1],&p,&e)){
    fprintf(stderr,"provider line %zu: %s\n",e.line,e.message);return 1;
  }
  unsigned limit=p.max_results<20?p.max_results:20;
  if(!mulib_elvi_scrape_url(&p,argv[2],limit,url,sizeof url))return 1;
  printf("request: %s\n",url);
  f=fopen(argv[3],"rb");if(!f){perror(argv[3]);return 1;}
  if(fseek(f,0,SEEK_END)!=0 || (n=ftell(f))<0 || n>2097152 ||
     fseek(f,0,SEEK_SET)!=0){fclose(f);return 1;}
  html=(unsigned char*)malloc((size_t)n+1);
  if(!html){fclose(f);return 1;}
  if(fread(html,1,(size_t)n,f)!=(size_t)n || ferror(f)){free(html);fclose(f);return 1;}
  fclose(f);html[n]=0;
  if(!mulib_elvi_scrape_parse(&p,(char*)html,(size_t)n,results,20,&count)){
    free(html);fprintf(stderr,"HTML parse failed\n");return 1;
  }
  for(size_t i=0;i<count;i++) printf("%zu. %s\n   %s\n",i+1,results[i].title,results[i].url);
  free(html);return 0;
}
