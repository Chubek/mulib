#define MULIB_IMPL_DESCRITORS
#include "mulib-descritors.h"
#include <assert.h>
int main(void) {
    uint8_t code[8], gate[16], gdtr[10];
    assert(!mulib_descritors_segment(code, 0, 0xfffff, 0x9a, 10));
    assert(!mulib_descritors_gate(gate, UINT64_C(0xffff800000001000), 8, 0, 0, 14));
    assert(!mulib_descritors_pointer(gdtr, (uint64_t)(uintptr_t)code, sizeof(code)));
    return 0;
}
