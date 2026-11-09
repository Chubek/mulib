/* Focused executable regression for mulib-eda-project.h. */
#define MULIB_IMPL_EDA_PROJECT
#include "mulib-eda-project.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_project_t p;assert(mulib_eda_project_init(&p,"Amb3r demo","r1",2,42));assert(p.seed==42&&p.board_variants==2);assert(!mulib_eda_project_init(&p,"", "r1",1,1));
    puts("PASS mulib-eda-project");
    return 0;
}
