#define MULIB_IMPL_SCRAPE_READABLE
#include "mulib-scrape-readable.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
char b[80];mulib_scrape_readable("<p>hello  <b>world</b></p>",b,sizeof b);assert(!strcmp(b,"hello world"));
return 0;
}
