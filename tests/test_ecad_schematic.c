/* Focused executable regression for mulib-ecad-schematic.h. */
#define MULIB_IMPL_ECAD_SCHEMATIC
#include "mulib-ecad-schematic.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_ecad_schematic_t s={0};mulib_ecad_component_t c={"R1",'R',"in","out",1000};assert(mulib_ecad_schematic_add(&s,&c)&&s.count==1);assert(!mulib_ecad_schematic_add(&s,&c));
    puts("PASS mulib-ecad-schematic");
    return 0;
}
