/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docir.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCIR
#include "mulib-docir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docir_node_t *r=mulib_docir_new(mulib_docir_ROOT,NULL),*p=mulib_docir_new(mulib_docir_PARAGRAPH,"Hi & Bye");assert(r&&p);assert(mulib_docir_append(r,p));mulib_typecore_buf_t b={0};assert(mulib_docir_html(r,&b));assert(mulib_typecore_ends(&b));assert(strstr((char*)b.data,"Hi &amp; Bye"));mulib_typecore_free(&b);mulib_docir_destroy(r);return 0;}
