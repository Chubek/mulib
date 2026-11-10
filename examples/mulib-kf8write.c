/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-kf8write.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_KF8WRITE
#include "mulib-kf8write.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_kf8write_flow_t f[]={ {0,10},{10,20} };mulib_typecore_buf_t b={0};assert(mulib_kf8write_fdst(f,2,&b));assert(b.size==28&&!memcmp(b.data,"FDST",4));mulib_typecore_free(&b);return 0;}
