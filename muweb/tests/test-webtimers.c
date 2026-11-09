#define MULIB_IMPL_WEBTIMERS
#include "mulib-webtimers.h"
#include <assert.h>
#include <string.h>
int main(void) {
mulib_webtimers_t t={0};uint64_t id=mulib_webtimers_add(&t,100,20);assert(id);assert(!mulib_webtimers_next(&t,110));assert(mulib_webtimers_next(&t,120)==id);
return 0;
}
