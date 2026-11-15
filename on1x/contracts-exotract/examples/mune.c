/* Separate caller TU: link the generated one-TU libmulib.a, not another implementation. */
#include "../../tests/fixtures/mune_semantics.c"
#include <stdio.h>

int main(void)
{
    assert(on1x_mune_contract_semantics() == 1);
    puts("mune: archive OK");
    return 0;
}
