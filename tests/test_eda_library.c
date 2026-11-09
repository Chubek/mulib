/* Focused executable regression for mulib-eda-library.h. */
#define MULIB_IMPL_EDA_LIBRARY
#include "mulib-eda-library.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_library_t lib={0};mulib_eda_partdef_t p={"R_0603","Chip resistor",2};assert(mulib_eda_library_register(&lib,&p));assert(mulib_eda_library_find(&lib,"R_0603")->pins==2);assert(!mulib_eda_library_register(&lib,&p));mulib_eda_library_free(&lib);
    puts("PASS mulib-eda-library");
    return 0;
}
