/* Checked usage example for mulib-eda-object.h. */
#define MULIB_IMPL_EDA_IDS
#define MULIB_IMPL_EDA_OBJECT
#include "mulib-eda-object.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_eda_object_t o;
    assert(mulib_eda_object_init(&o, 123, 1, "R1"));
    assert(mulib_eda_object_rename(&o, "R2"));
    assert(o.revision == 2 && !strcmp(o.name, "R2") && o.id == 123);
    assert(!mulib_eda_object_init(&o, 0, 1, "bad"));
    puts("PASS mulib-eda-object");
    return 0;
}
