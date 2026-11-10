/* Standalone API contract test for mulib-palmdb. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PALMDB
#include "mulib-palmdb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){const char d[]="hello";mulib_palmdb_record_t r={d,5};mulib_typecore_buf_t b={0};assert(mulib_palmdb_write("test","TEXt","REAd",&r,1,&b));assert(b.size>85&&b.data[76]==0&&b.data[77]==1);mulib_typecore_free(&b);return 0;}
