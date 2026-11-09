/* Focused executable regression for mulib-eda-undo.h. */
#define MULIB_IMPL_EDA_DESIGN_DB
#define MULIB_IMPL_EDA_UNDO
#include "mulib-eda-undo.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_design_db_t db={0};mulib_eda_undo_t undo={0};mulib_eda_db_record_t r={1,1,2};assert(mulib_eda_db_put(&db,r));assert(mulib_eda_undo_apply(&undo,&db,1,3));assert(mulib_eda_undo_back(&undo,&db));assert(mulib_eda_db_get(&db,1,&r)&&r.value==2);assert(mulib_eda_undo_forward(&undo,&db));assert(mulib_eda_db_get(&db,1,&r)&&r.value==3);mulib_eda_db_free(&db);
    puts("PASS mulib-eda-undo");
    return 0;
}
