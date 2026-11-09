#define MULIB_IMPL_MIME
#include "mulib-mime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
char b[80];assert(mulib_mime_type("Text/HTML; charset=utf-8",b,sizeof b));assert(!strcmp(b,"text/html"));
return 0;
}
