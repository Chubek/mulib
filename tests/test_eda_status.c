/* Focused executable regression for mulib-eda-status.h. */
#define MULIB_IMPL_EDA_STATUS
#include "mulib-eda-status.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_diagnostic_t d={0};mulib_eda_diagnostic_set(&d,MULIB_EDA_INVALID,2,3,"bad pin");assert(d.line==2&&d.column==3&&strcmp(d.message,"bad pin")==0);assert(!strcmp(mulib_eda_code_name(MULIB_EDA_NOMEM),"NOMEM"));assert(!strcmp(mulib_eda_code_name((mulib_eda_code_t)-999),"UNKNOWN"));
    puts("PASS mulib-eda-status");
    return 0;
}
