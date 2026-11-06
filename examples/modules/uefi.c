#define MULIB_IMPL_UEFI
#include "mulib-uefi.h"
#include <assert.h>
int main(void) {
    uint8_t raw[48] = {7}; mulib_uefi_memory_t memory;
    raw[24] = 4;
    assert(!mulib_uefi_memory_at(raw, sizeof(raw), 48, 0, &memory));
    assert(memory.type == MULIB_UEFI_CONVENTIONAL_MEMORY && memory.pages == 4);
    assert(mulib_uefi_is_error(MULIB_UEFI_BUFFER_TOO_SMALL));
    return 0;
}
