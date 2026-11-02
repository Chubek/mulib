#define MULIB_IMPL_EFI
#include "mulib-efi.h"
#include <assert.h>
int main(void) {
    uint8_t raw[40] = {7}; mulib_efi_memory_t memory;
    raw[24] = 16;
    assert(!mulib_efi_memory_at(raw, sizeof(raw), 40, 0, &memory));
    assert(memory.type == 7 && memory.pages == 16);
    return 0;
}
