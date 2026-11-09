#include "../notation-common.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    mulib_chem_formula_t *h=mulib_chem_parse("H2",2,NULL),*o=mulib_chem_parse("O2",2,NULL),*water=mulib_chem_parse("H2O",3,NULL);
    const mulib_chem_formula_t *species[]={h,o,water};uint64_t coefficients[3];
    assert(h&&o&&water&&mulib_chem_balance(species,2,1,coefficients,NULL));
    printf("%llu H2 + %llu O2 -> %llu H2O; water approximately %.3f g/mol\n",(unsigned long long)coefficients[0],(unsigned long long)coefficients[1],(unsigned long long)coefficients[2],mulib_chem_molar_mass(water));
    mulib_chem_destroy(h);mulib_chem_destroy(o);mulib_chem_destroy(water);return 0;
}
