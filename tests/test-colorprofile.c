/* Standalone API contract test for mulib-colorprofile. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_COLORPROFILE
#include "mulib-colorprofile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_colorprofile_table_t t={0}; assert(mulib_colorprofile_set(&t,"example","first"));assert(mulib_colorprofile_set(&t,"example","second")); assert(!strcmp(mulib_colorprofile_get(&t,"example"),"second"));assert(mulib_colorprofile_erase(&t,"example")); assert(!mulib_colorprofile_get(&t,"example")); mulib_colorprofile_destroy(&t);return 0;}
