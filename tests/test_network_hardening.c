/* Regression tests for the networking modules; no privileged operations. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_EXTACK
#define MULIB_IMPL_NETLINK_SCHEMA
#define MULIB_IMPL_NETLINK_ASYNC
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_SOCK_DIAG
#define MULIB_IMPL_INET_DIAG
#define MULIB_IMPL_UIP_ARP_CACHE
#define MULIB_IMPL_UIP_FRAGMENT
#define MULIB_IMPL_UIP_PBUF
#define MULIB_IMPL_UIP_DNS
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_ICMP
#define MULIB_IMPL_WIFI_SECURITY
#define MULIB_IMPL_NET_PROFILE
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_PROPERTIES
#include "mulib-netlink-extack.h"
#include "mulib-netlink-schema.h"
#include "mulib-netlink-async.h"
#include "mulib-rtnl.h"
#include "mulib-inet-diag.h"
#include "mulib-uip-arp-cache.h"
#include "mulib-uip-fragment.h"
#include "mulib-uip-pbuf.h"
#include "mulib-uip-dns.h"
#include "mulib-uip-checksum.h"
#include "mulib-icmp.h"
#include "mulib-wifi-security.h"
#include "mulib-net-profile.h"
#include "mulib-dbus-properties.h"

static int
count_message (const void *p, size_t n, void *ctx)
{
  (void)p;
  (void)n;
  ++*(unsigned *)ctx;
  return 0;
}

static void
test_netlink_parsers (void)
{
  uint8_t raw[512] = { 0 };
  /* Public byte-oriented parsers must accept unaligned input. */
  struct nlmsghdr h = { 0 };
  h.nlmsg_len = sizeof h;
  memcpy (raw + 1, &h, sizeof h);
  unsigned count = 0;
  assert (!mulib_netlink_foreach (raw + 1, sizeof h, count_message, &count));
  assert (count == 1);
  h.nlmsg_len = sizeof h - 1;
  memcpy (raw + 1, &h, sizeof h);
  assert (mulib_netlink_foreach (raw + 1, sizeof h, count_message, &count)
          == -EBADMSG);

  size_t used = 0;
  uint32_t value = 42;
  assert (!mulib_netlink_attr_append (raw + 1, sizeof raw - 1, &used,
                                      7 | NLA_F_NESTED, &value, sizeof value));
  mulib_netlink_policy_t policy = { 7, 4, 4, 1 };
  assert (!mulib_netlink_schema_check (raw + 1, used, &policy, 1));
  policy.minimum = 5;
  assert (mulib_netlink_schema_check (raw + 1, used, &policy, 1) == -EINVAL);
  mulib_netlink_attr_t invalid = { 7, NULL, 4 };
  assert (mulib_netlink_attr_u32 (&invalid, &value) == -EINVAL);

  /* An uncapped error includes the entire original request before TLVs. */
  memset (raw, 0, sizeof raw);
  h.nlmsg_type = NLMSG_ERROR;
  h.nlmsg_flags = NLM_F_ACK_TLVS;
  struct nlmsgerr e = { 0 };
  e.error = -EINVAL;
  e.msg.nlmsg_len = NLMSG_HDRLEN + 8;
  used = NLMSG_LENGTH (sizeof e) + 8;
  const char text[] = "invalid request";
  assert (!mulib_netlink_attr_append (raw + 1, sizeof raw - 1, &used,
                                      NLMSGERR_ATTR_MSG, text, sizeof text));
  h.nlmsg_len = (uint32_t)used;
  memcpy (raw + 1, &h, sizeof h);
  memcpy (raw + 1 + NLMSG_HDRLEN, &e, sizeof e);
  mulib_netlink_error_t error;
  assert (!mulib_netlink_extack_parse (raw + 1, used, &error));
  assert (error.code == -EINVAL && !strcmp (error.message, text));
  /* Truncated echoed request must not be mistaken for TLVs. */
  e.msg.nlmsg_len = 1000;
  memcpy (raw + 1 + NLMSG_HDRLEN, &e, sizeof e);
  assert (mulib_netlink_extack_parse (raw + 1, used, &error) == -EBADMSG);

  memset (raw, 0, sizeof raw);
  h.nlmsg_flags = NLM_F_CAPPED | NLM_F_ACK_TLVS;
  e.msg.nlmsg_len = NLMSG_HDRLEN;
  used = NLMSG_LENGTH (sizeof e);
  char long_text[300];
  memset (long_text, 'x', sizeof long_text);
  long_text[sizeof long_text - 1] = 0;
  assert (!mulib_netlink_attr_append (raw + 1, sizeof raw - 1, &used,
                                      NLMSGERR_ATTR_MSG, long_text,
                                      sizeof long_text));
  h.nlmsg_len = (uint32_t)used;
  memcpy (raw + 1, &h, sizeof h);
  memcpy (raw + 1 + NLMSG_HDRLEN, &e, sizeof e);
  assert (!mulib_netlink_extack_parse (raw + 1, used, &error));
  assert (strlen (error.message) == 255);

  /* Ignore bytes outside the declared message, even if the buffer is longer.
   */
  h.nlmsg_type = RTM_NEWLINK;
  h.nlmsg_flags = 0;
  used = NLMSG_LENGTH (sizeof (struct ifinfomsg));
  size_t inner = 0;
  uint8_t attrs[32];
  assert (!mulib_netlink_attr_append (attrs, sizeof attrs, &inner,
                                      IFLA_INFO_KIND, "bridge", 7));
  h.nlmsg_len = (uint32_t)used;
  assert (!mulib_netlink_attr_append (raw + 1, sizeof raw - 1, &used,
                                      IFLA_LINKINFO, attrs, inner));
  memcpy (raw + 1, &h, sizeof h);
  assert (!mulib_rtnl_link_kind (raw + 1, used, "bridge"));
  h.nlmsg_len = (uint32_t)used;
  memcpy (raw + 1, &h, sizeof h);
  assert (mulib_rtnl_link_kind (raw + 1, used, "bridge"));

  h.nlmsg_type = SOCK_DIAG_BY_FAMILY;
  h.nlmsg_len = NLMSG_HDRLEN;
  memcpy (raw + 1, &h, sizeof h);
  mulib_inet_diag_socket_t socket_info;
  assert (mulib_inet_diag_decode (raw + 1, sizeof raw - 1, &socket_info)
          == -EBADMSG);
}

static void
test_cache_and_fragments (void)
{
  mulib_uip_arp_cache_t cache = { 0 };
  const uint8_t a[4] = { 10, 0, 0, 1 }, b[4] = { 10, 0, 0, 2 };
  const uint8_t old_mac[6] = { 2, 0, 0, 0, 0, 1 },
                new_mac[6] = { 2, 0, 0, 0, 0, 2 };
  uint8_t mac[6];
  assert (!mulib_uip_arp_cache_store (&cache, a, old_mac, 0, 1));
  assert (!mulib_uip_arp_cache_store (&cache, b, old_mac, 0, 100));
  /* Updating b must search past an earlier expired slot. */
  assert (!mulib_uip_arp_cache_store (&cache, b, new_mac, 2, 100));
  unsigned matches = 0;
  for (unsigned i = 0; i < 32; ++i)
    matches += cache.entries[i].valid && !memcmp (cache.entries[i].ip, b, 4);
  assert (matches == 1);
  assert (!mulib_uip_arp_cache_find (&cache, b, 2, mac));
  assert (!memcmp (mac, new_mac, 6));
  assert (!mulib_uip_arp_cache_find (&cache, b, 101, mac));
  assert (mulib_uip_arp_cache_find (&cache, b, 102, mac) == -ENOENT);
  assert (mulib_uip_arp_cache_store (&cache, a, old_mac, UINT64_MAX, 1)
          == -EINVAL);
  /* Saturated expirations still need a deterministic eviction candidate. */
  for (unsigned i = 0; i < 32; ++i)
    {
      cache.entries[i].valid = 1;
      cache.entries[i].expires_at = UINT64_MAX;
      memset (cache.entries[i].ip, 0, 4);
      cache.entries[i].ip[3] = (uint8_t)i;
    }
  assert (!mulib_uip_arp_cache_store (&cache, a, new_mac, 0, UINT64_MAX));
  assert (!memcmp (cache.entries[0].ip, a, 4));

  mulib_uip_frag_t f = { 0 };
  const uint8_t data[16] = { 1 };
  size_t complete = 99;
  assert (!mulib_uip_frag_push (&f, a, b, 17, 1, 16, 1, data, 8, &complete));
  assert (mulib_uip_frag_push (&f, a, b, 17, 1, 0, 0, data, 8, &complete)
          == -EBADMSG);
  assert (!f.has_last);
  mulib_uip_frag_reset (&f);
  assert (!mulib_uip_frag_push (&f, a, b, 17, 1, 0, 1, data, 16, &complete));
  assert (mulib_uip_frag_push (&f, a, b, 17, 1, 8, 0, data, 8, &complete)
          == -EEXIST);
  assert (!f.has_last);
  assert (mulib_uip_frag_push (&f, a, b, 17, 1, 16, 0, data, 8, &complete)
          == 1);
  assert (complete == 24);

  mulib_uip_pbuf_t p = { 0 };
  assert (!mulib_uip_pbuf_init (&p, 8, 16));
  assert (!mulib_uip_pbuf_append (&p, data, 8));
  assert (!mulib_uip_pbuf_prepend (&p, data, 8));
  assert (p.length == 16);
  p.start = 17;
  assert (!mulib_uip_pbuf_data (&p));
  assert (mulib_uip_pbuf_prepend (&p, data, 1) == -EINVAL);
  mulib_uip_pbuf_destroy (&p);
}

static void
test_codecs (void)
{
  /* More than 2^32 accumulated checksum bits used to lose carries. */
  uint8_t *large = malloc (200001);
  assert (large);
  memset (large, 0xff, 200001);
  assert (mulib_uip_checksum (large, 200000) == 0);
  assert (mulib_icmp_checksum (large, 200000) == 0);
  assert (mulib_uip_checksum (large, 200001) == 0xff);
  free (large);

  uint8_t dns[64] = { 0 };
  dns[1] = 42;
  dns[2] = 0x81;
  dns[5] = 1;
  dns[7] = 1;
  dns[12] = 1;
  dns[13] = 'a';
  dns[14] = 0;
  dns[16] = 1;
  dns[18] = 1;
  dns[19] = 0xc0;
  dns[20] = 12;
  dns[22] = 1;
  dns[24] = 1;
  dns[30] = 4;
  dns[31] = 1;
  dns[32] = 2;
  dns[33] = 3;
  dns[34] = 4;
  uint8_t ip[4] = { 0 };
  assert (!mulib_uip_dns_first_a (dns, 35, 42, ip));
  assert (ip[3] == 4);
  dns[20] = 63;
  assert (mulib_uip_dns_first_a (dns, 35, 42, ip) == -EBADMSG);
  dns[20] = 19;
  assert (mulib_uip_dns_first_a (dns, 35, 42, ip) == -EBADMSG);
  dns[20] = 12;
  dns[2] |= 2;
  assert (mulib_uip_dns_first_a (dns, 35, 42, ip) == -EBADMSG);

  uint8_t rsn[] = { 48, 18,  1, 0, 0, 15, 172, 4,   1, 0, 0,
                    15, 172, 4, 1, 0, 0,  15,  172, 2, 0 };
  mulib_wifi_security_rsn_t security;
  assert (!mulib_wifi_security_rsn_parse (rsn, sizeof rsn - 1, &security));
  rsn[1]++;
  assert (mulib_wifi_security_rsn_parse (rsn, sizeof rsn, &security)
          == -EBADMSG);
  rsn[1]--;
  rsn[8] = 0;
  assert (mulib_wifi_security_rsn_parse (rsn, sizeof rsn - 1, &security)
          == -EBADMSG);

  mulib_net_profile_t profile;
  const char embedded[] = "interface=lo\0hidden\nenabled=1";
  assert (mulib_net_profile_parse (embedded, sizeof embedded - 1, &profile)
          == -EINVAL);
}

static void
test_dbus (void)
{
  uint8_t reply[64] = { 'l', 2, 0, 1 };
  reply[8] = 1;
  reply[12] = 8;
  reply[16] = 5;
  reply[17] = 1;
  reply[18] = 'u';
  reply[20] = 7;
  assert (!mulib_dbus_client_reply_ (reply, 24, 7));
  assert (mulib_dbus_client_reply_ (reply, 24, 8) == 1);
  reply[17] = 0;
  assert (mulib_dbus_client_reply_ (reply, 24, 7) == -EBADMSG);
  reply[17] = 1;
  reply[19] = 1;
  assert (mulib_dbus_client_reply_ (reply, 24, 7) == -EBADMSG);
  reply[19] = 0;
  reply[24] = 1;
  reply[25] = 'u';
  reply[28] = 70;
  uint32_t value = 123;
  /* Trailing bytes aren't part of a zero-length declared body. */
  assert (mulib_dbus_properties_extract_ (reply, 32, 'u', &value, sizeof value)
          == -EBADMSG);
  assert (value == 123);
  reply[4] = 8;
  assert (
      !mulib_dbus_properties_extract_ (reply, 32, 'u', &value, sizeof value));
  assert (value == 70);
  int pair[2];
  assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
  assert (write (pair[0], reply, 32) == 32);
  uint8_t received[64];
  size_t n = 0;
  assert (!mulib_dbus_transport_read (pair[1], received, sizeof received, &n));
  assert (n == 32 && !memcmp (reply, received, n));
  close (pair[0]);
  close (pair[1]);

  assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
  close (pair[0]);
  assert (mulib_netlink_async_wait (pair[1], 0, 1) == -EPIPE);
  /* A ready EOF is readable; callers can then handle end-of-stream. */
  assert (!mulib_netlink_async_wait (pair[1], 0, 0));
  close (pair[1]);
}

static void
test_auth (void)
{
  const char *responses[] = {
    "OK 0123456789abcdef0123456789abcdef\r\n",
    "OK short\r\n",
    "OK 0123456789abcdef0123456789abcdeg\r\n",
    "OK 0123456789abcdef0123456789abcdef\n"
  };
  for (unsigned i = 0; i < sizeof responses / sizeof responses[0]; ++i)
    {
      int pair[2];
      assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
      size_t len = strlen (responses[i]);
      assert (write (pair[1], responses[i], len) == (ssize_t)len);
      assert (mulib_dbus_auth_external (pair[0]) == (i ? -EACCES : 0));
      char sent[256];
      ssize_t n = recv (pair[1], sent, sizeof sent, MSG_DONTWAIT);
      assert (n > 0 && sent[0] == 0);
      /* Failed authentication must not switch to binary protocol. */
      assert ((n >= 7 && !memcmp (sent + n - 7, "BEGIN\r\n", 7)) == !i);
      close (pair[0]);
      close (pair[1]);
    }
}

int
main (void)
{
  test_netlink_parsers ();
  test_cache_and_fragments ();
  test_codecs ();
  test_dbus ();
  test_auth ();
  puts ("PASS networking hardening");
  return 0;
}
