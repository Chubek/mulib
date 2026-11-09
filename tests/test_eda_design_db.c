/* Focused executable regression for mulib-eda-design-db.h. */
#define MULIB_IMPL_EDA_DESIGN_DB
#include "mulib-eda-design-db.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_design_db_t db={0};mulib_eda_db_record_t r={3,7,10},o={0};assert(mulib_eda_db_put(&db,r));assert(mulib_eda_db_get(&db,3,&o)&&o.value==10);r.value=22;assert(mulib_eda_db_put(&db,r)&&db.count==1&&db.epoch==2);assert(mulib_eda_db_delete(&db,3));assert(!mulib_eda_db_get(&db,3,&o));mulib_eda_db_free(&db);
    puts("PASS mulib-eda-design-db");
    return 0;
}
