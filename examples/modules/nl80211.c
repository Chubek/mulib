/* Wireless hardware is optional; lookup failures are reported, not hidden. */
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_NL80211
#include "mulib-nl80211.h"
#include <stdio.h>
#include <linux/nl80211.h>
int main(void) { int r = mulib_nl80211_command(NL80211_CMD_GET_INTERFACE, 0, NLM_F_DUMP, NULL, NULL); printf("interface query status=%d\n", r); return 0; }
