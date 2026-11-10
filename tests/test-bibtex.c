/* Standalone API contract test for mulib-bibtex. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_BIBTEX
#include "mulib-bibtex.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){char type[16],key[32];const char *s="@article{knuth84, title={TeX}}";assert(mulib_bibtex_entry(s,strlen(s),type,sizeof(type),key,sizeof(key)));assert(!strcmp(key,"knuth84"));return 0;}
