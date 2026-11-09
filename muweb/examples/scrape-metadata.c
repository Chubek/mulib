#define MULIB_IMPL_SCRAPE_METADATA
#include "mulib-scrape-metadata.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
char b[80];assert(mulib_scrape_metadata_get("<meta name=\"description\" content=\"hello\">","description",b,sizeof b));assert(!strcmp(b,"hello"));
return 0;
}
