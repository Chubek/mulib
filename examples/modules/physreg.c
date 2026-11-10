#define MULIB_IMPL_PHYSREG
#include "mulib-physreg.h"
#include <assert.h>
int main(void) {
    mulib_physreg_t registers={15,16,3,12};unsigned index;
    assert(mulib_physreg_validate(&registers));
    assert(mulib_physreg_pick(&registers,3,&index)&&index==2);return 0;
}
