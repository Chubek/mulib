/* Focused executable regression for mulib-eda-hierarchy.h. */
#define MULIB_IMPL_EDA_HIERARCHY
#include "mulib-eda-hierarchy.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_hierarchy_t h={0};uint32_t a,b;char path[128];assert(mulib_eda_hierarchy_add(&h,UINT32_MAX,"top",&a));assert(mulib_eda_hierarchy_add(&h,a,"u1",&b));assert(mulib_eda_hierarchy_path(&h,b,path,sizeof path)&&!strcmp(path,"top/u1"));assert(!mulib_eda_hierarchy_add(&h,a,"u1",NULL));assert(!mulib_eda_hierarchy_add(&h,b,"bad/name",NULL));mulib_eda_hierarchy_free(&h);
    puts("PASS mulib-eda-hierarchy");
    return 0;
}
