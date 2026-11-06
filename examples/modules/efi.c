#define MULIB_IMPL_EFI
#define MULIB_IMPL_PE
#define MULIB_IMPL_COFF
#define MULIB_IMPL_ENDIAN
#include "mulib-efi.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_efi_image_t image;
    assert(!strcmp(mulib_efi_boot_name(0x8664), "BOOTX64.EFI"));
    assert(mulib_efi_open(&image, "not a PE", 8) == MULIB_PE_EFORMAT);
    return 0;
}
