#define MULIB_IMPL_SCRAPE_SCHEMA
#include "mulib-scrape-schema.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
assert(mulib_scrape_schema_string("abc",1,4));assert(!mulib_scrape_schema_string("",1,4));
return 0;
}
