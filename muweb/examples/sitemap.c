#define MULIB_IMPL_SITEMAP
#include "mulib-sitemap.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
char b[128];size_t p=0;const char *s="<url><loc>https://x/</loc></url>";assert(mulib_sitemap_next(s,strlen(s),&p,b,sizeof b));assert(!strcmp(b,"https://x/"));
return 0;
}
