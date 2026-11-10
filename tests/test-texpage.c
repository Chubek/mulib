/* Standalone API contract test for mulib-texpage. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXPAGE
#include "mulib-texpage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double h[]={10,20,30,40};mulib_texpage_pages_t p={0};assert(mulib_texpage_paginate(h,4,50,&p));assert(p.count==3&&p.ends[0]==2);mulib_texpage_destroy(&p);return 0;}
