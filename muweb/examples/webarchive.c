#define MULIB_IMPL_WEBARCHIVE
#include "mulib-webarchive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
char b[256];assert(mulib_webarchive_record_header("https://a",3,b,sizeof b));assert(strstr(b,"WARC/1.0"));
return 0;
}
