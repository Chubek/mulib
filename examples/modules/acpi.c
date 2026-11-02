#define MULIB_IMPL_ACPI
#include "mulib-acpi.h"
#include <assert.h>
int main(void) {
    uint8_t table[36] = {'T', 'E', 'S', 'T', 36}; size_t length, i;
    unsigned sum = 0;
    for (i = 0; i < sizeof(table); ++i) sum += table[i];
    table[9] = (uint8_t)(0u - sum);
    assert(!mulib_acpi_sdt(table, sizeof(table), "TEST", &length) && length == 36);
    return 0;
}
