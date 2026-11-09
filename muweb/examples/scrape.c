#define MULIB_IMPL_SCRAPE
#include "mulib-scrape.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
char b[64];assert(mulib_scrape_text("<b>abc</b>",b,sizeof b)==3);assert(!strcmp(b,"abc"));
return 0;
}
