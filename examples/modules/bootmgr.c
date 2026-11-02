#define MULIB_IMPL_BOOTMGR
#include "mulib-bootmgr.h"
#include <assert.h>
int main(void) {
    mulib_bootmgr_entry_t entries[] = {
        {"OS", "\\EFI\\BOOT\\BOOTX64.EFI", "", MULIB_BOOTMGR_EFI, 1, 20},
        {"Recovery", "recovery", "", MULIB_BOOTMGR_KERNEL, 1, 10}
    };
    assert(mulib_bootmgr_select(entries, 2) == 0);
    return 0;
}
