#define MULIB_IMPL_PCI
#include "mulib-pci.h"
#include <assert.h>
int main(void) {
    /* Relative to the MCFG segment/start-bus base supplied by the caller. */
    assert(mulib_pci_ecam(0, 1, 0, 0x10) == 0x8010);
    assert(mulib_pci_ecam(0, 32, 0, 0) == UINT64_MAX);
    return 0;
}
