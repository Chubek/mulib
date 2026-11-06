#define MULIB_IMPL_UEFI
#define MULIB_IMPL_UEFIUTILS
#include "mulib-uefiutils.h"
#include <assert.h>
int main(void) {
    uint16_t name[9], path[] = {'\\', 'a', '.', 'e', 'f', 'i'};
    uint8_t wire[64]; size_t bytes;
    assert(!mulib_uefiutils_boot_name(42, name) && name[7] == 'A');
    assert(!mulib_uefiutils_file_path(path, 6, wire, sizeof(wire), &bytes));
    assert(!mulib_uefiutils_path_check(wire, bytes));
    return 0;
}
