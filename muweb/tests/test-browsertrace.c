#define MULIB_IMPL_BROWSERTRACE
#include "mulib-browsertrace.h"
#include <assert.h>
#include <string.h>
int main(void) {
mulib_browsertrace_t t={0};mulib_browsertrace_push(&t,(mulib_browsertrace_event_t){10,1,2});assert(mulib_browsertrace_get(&t,0)->time_ms==10);
return 0;
}
