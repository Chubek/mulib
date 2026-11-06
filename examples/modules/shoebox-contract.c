#define MULIB_IMPL_SHOEBOX_CONTRACT
#include "mulib-shoebox-contract.h"
#include <assert.h>
#include <string.h>
int main(void) {
    const char *env[] = {"PATH=/bin:/sbin"};
    mulib_shoebox_context_t context = {0};
    context.abi_version = MULIB_SHOEBOX_ABI_VERSION; context.struct_size = sizeof(context);
    context.environment = env; context.env_count = 1;
    assert(!mulib_shoebox_validate(NULL, 0)); /* No utilities registered yet. */
    assert(!strcmp(mulib_shoebox_getenv(&context, "PATH"), "/bin:/sbin"));
    return 0;
}
