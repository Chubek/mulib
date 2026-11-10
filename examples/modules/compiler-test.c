#define MULIB_IMPL_COMPILER_TEST
#include "mulib-compiler-test.h"
#include <assert.h>
int main(void) {
    mulib_compiler_test_t tests={0};
    assert(mulib_compiler_test_check(&tests,6*7==42,"constant folding"));
    assert(mulib_compiler_test_ok(&tests)&&tests.passed==1);
    return 0;
}
