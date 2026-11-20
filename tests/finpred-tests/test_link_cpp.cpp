#include "mulib-contract.h"
#include "mulib-pricecurve.h"
#include <cassert>
int main(){
    mulib_pricecurve_config_t c{};
    c.kind=MULIB_PRICECURVE_LINEAR;c.near_percentile=0.4;
    c.far_percentile=0.8;c.horizon_days=90;
    double x=0;
    assert(mulib_pricecurve_evaluate(&c,45,&x));
    assert(x>0.59 && x<0.61);
    return 0;
}
