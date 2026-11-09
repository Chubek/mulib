#define MULIB_IMPL_SCRAPE_TABLE
#include "mulib-scrape-table.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
assert(mulib_scrape_table_cells("<table><tr><th>X</th><td>Y</td></tr></table>")==2);
return 0;
}
