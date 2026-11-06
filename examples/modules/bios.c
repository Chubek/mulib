#define MULIB_IMPL_BIOS
#include "mulib-bios.h"
#include <assert.h>
int main(void) {
    uint8_t packet[16];
    assert(!mulib_bios_dap(packet, sizeof(packet), 2048, 1, 0x1000, 0, 512));
    assert(packet[0] == 16 && packet[2] == 1 && packet[9] == 8);
    return 0;
}
