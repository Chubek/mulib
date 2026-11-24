#include <inttypes.h>
#include <stdio.h>
#include "mulib-container-core.h"

int main(void)
{
    printf("2\n40\n");
    printf("%" PRIu64 "\n", mulib_ct_counter_add(UINT64_C(2), UINT64_C(40)));
    printf("%" PRIu64 "\n", mulib_ct_counter_add(UINT64_MAX, UINT64_C(1)));
    printf("%" PRIu64 "\n",
           mulib_ct_counter_add(UINT64_C(9223372036854775808), UINT64_C(9223372036854775806)));
    printf("%" PRIu64 "\n", mulib_ct_counter_add(UINT64_C(0), UINT64_C(0)));
    return 0;
}
