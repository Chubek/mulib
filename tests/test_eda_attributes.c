/* Focused executable regression for mulib-eda-attributes.h. */
#define MULIB_IMPL_EDA_ATTRIBUTES
#include "mulib-eda-attributes.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_attrs_t a={0};mulib_eda_attr_t x={0};x.owner=3;x.type=MULIB_EDA_ATTR_STRING;strcpy(x.key,"footprint");strcpy(x.text,"0603");assert(mulib_eda_attrs_set(&a,&x));assert(!strcmp(mulib_eda_attrs_get(&a,3,"footprint")->text,"0603"));strcpy(x.text,"0402");assert(mulib_eda_attrs_set(&a,&x)&&a.count==1);assert(!mulib_eda_attrs_get(&a,4,"footprint"));mulib_eda_attrs_free(&a);
    puts("PASS mulib-eda-attributes");
    return 0;
}
