/* Opens, but never enters, the current namespace. */
#define MULIB_IMPL_NETNS
#include "mulib-netns.h"
#include <assert.h>
int main(void) { int fd = mulib_netns_open_self(); assert(fd >= 0); close(fd); return 0; }
