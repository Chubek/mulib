/* MuLIB compiler tools: Deterministic compiler-test counter and named check harness.
 * ISO C11 single-header module. Define MULIB_IMPL_COMPILER_TEST in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_COMPILER_TEST_H
#define MULIB_COMPILER_TEST_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t passed,failed;
const char *last_failure;
} mulib_compiler_test_t;
int mulib_compiler_test_check(mulib_compiler_test_t *t,int condition,const char *name);
int mulib_compiler_test_ok(const mulib_compiler_test_t *t);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_COMPILER_TEST_H */
#if defined(MULIB_IMPL_COMPILER_TEST) && !defined(MULIB_COMPILER_TEST_IMPLEMENTED)
#define MULIB_COMPILER_TEST_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_compiler_test_check(mulib_compiler_test_t *t,int condition,const char *name){if(!t||!name)return 0;
if(condition){if(t->passed==SIZE_MAX)return 0;
t->passed++;
return 1;
}if(t->failed==SIZE_MAX)return 0;
t->failed++;
t->last_failure=name;
return 0;
}
int mulib_compiler_test_ok(const mulib_compiler_test_t *t){return t&&t->failed==0;
}
#ifdef __cplusplus
}
#endif
#endif
