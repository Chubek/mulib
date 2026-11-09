#define MULIB_IMPL_SEARCH_QUERY
#include "mulib-search-query.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
char b[256];assert(mulib_search_query_url("bing","a b",b,sizeof b));assert(strstr(b,"q=a+b"));
return 0;
}
