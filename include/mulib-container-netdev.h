/** @file mulib-container-netdev.h
 * @brief Bounded virtual packet endpoints with backpressure and link statistics.
 * Define MULIB_IMPL_CONTAINER_NETDEV once; transports are explicit callbacks. */
#ifndef MULIB_CONTAINER_NETDEV_H
#define MULIB_CONTAINER_NETDEV_H
#include "include-mulib-container-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_container_netdev_kind
{ MULIB_CONTAINER_NETDEV_IP = 1, MULIB_CONTAINER_NETDEV_ETHERNET = 2 } mulib_container_netdev_kind;
/** @brief Send one whole packet: zero accepted, AGAIN retry, negative CT error.
 * Callback borrows bytes only during call. */
typedef int (*mulib_container_netdev_send_fn) (void *, const void *, size_t);
typedef struct mulib_container_netdev
{
  char name[MULIB_CT_ID_CAP];
  mulib_container_netdev_kind kind;
  unsigned char mac[6];
  size_t mtu, queue_capacity, head, count, queued_bytes, max_queued_bytes;
  mulib_ct_blob *queue;
  mulib_container_netdev_send_fn transmit;
  void *user;
  uint64_t rx_packets, tx_packets, rx_bytes, tx_bytes, dropped;
  int up, busy;
} mulib_container_netdev;
/** @brief Allocate a receive ring. MTU 68..65535 is L3 bytes; Ethernet adds 14. */
int mulib_container_netdev_init (mulib_container_netdev *device, const char *name,
    mulib_container_netdev_kind kind, size_t mtu, size_t packets, size_t queued_bytes);
/** @brief Free receive storage; callback context remains caller-owned. */
void mulib_container_netdev_free (mulib_container_netdev *device);
/** @brief Configure borrowed transmit callback and optional locally administered MAC. */
int mulib_container_netdev_attach (mulib_container_netdev *device,
    mulib_container_netdev_send_fn transmit, void *user, const unsigned char mac[6]);
/** @brief Change link state; queued packets remain available after re-enabling. */
int mulib_container_netdev_link (mulib_container_netdev *device, int up);
/** @brief Copy an inbound packet; full queues return AGAIN without consuming data. */
int mulib_container_netdev_inject (mulib_container_netdev *device, const void *data, size_t length);
/** @brief Remove one inbound packet after successful copy. Too-small buffers
 * report required length and leave the packet queued. Empty queue returns AGAIN. */
int mulib_container_netdev_receive (mulib_container_netdev *device,
    void *data, size_t capacity, size_t *length);
/** @brief Send one packet atomically through the configured transport. */
int mulib_container_netdev_send (mulib_container_netdev *device, const void *data, size_t length);
/** @brief Transport callback suitable for connecting two endpoints. */
int mulib_container_netdev_peer (void *peer, const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_NETDEV) && !defined(MULIB_CONTAINER_NETDEV_IMPLEMENTED)
#define MULIB_CONTAINER_NETDEV_IMPLEMENTED
static int mulib_container_netdev_packet_ (const mulib_container_netdev *d, const void *p, size_t n)
{
  size_t header = d->kind == MULIB_CONTAINER_NETDEV_ETHERNET ? 14 : 0;
  if (!p || n <= header) return MULIB_CT_EINVAL;
  if (n > d->mtu + header) return MULIB_CT_ELIMIT;
  return 0;
}
int
mulib_container_netdev_init (mulib_container_netdev *d, const char *name,
    mulib_container_netdev_kind kind, size_t mtu, size_t count, size_t bytes)
{
  mulib_container_netdev next;
  if (!d || !mulib_ct_id_valid (name) || (kind != MULIB_CONTAINER_NETDEV_IP
      && kind != MULIB_CONTAINER_NETDEV_ETHERNET) || mtu < 68 || mtu > 65535
      || !count || count > SIZE_MAX / sizeof (*next.queue)) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  next.queue = (mulib_ct_blob *)MULIB_CT_MALLOC (count * sizeof (*next.queue));
  if (!next.queue) return MULIB_CT_ENOMEM;
  memset (next.queue, 0, count * sizeof (*next.queue));
  (void)mulib_ct_text_copy (next.name, sizeof (next.name), name);
  next.kind = kind; next.mtu = mtu; next.queue_capacity = count; next.max_queued_bytes = bytes;
  next.mac[0] = 2; next.mac[5] = 1; *d = next;
  return 0;
}
void mulib_container_netdev_free (mulib_container_netdev *d)
{
  size_t i;
  if (!d || d->busy) return;
  for (i = 0; i < d->queue_capacity; ++i) mulib_ct_blob_free (d->queue + i);
  MULIB_CT_FREE (d->queue); memset (d, 0, sizeof (*d));
}
int
mulib_container_netdev_attach (mulib_container_netdev *d,
    mulib_container_netdev_send_fn send, void *user, const unsigned char mac[6])
{
  unsigned char zero[6] = { 0 };
  if (!d || !d->queue_capacity || (mac && ((mac[0] & 1) || !memcmp (mac, zero, 6))))
    return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  d->transmit = send; d->user = user; if (mac) memcpy (d->mac, mac, 6);
  return 0;
}
int mulib_container_netdev_link (mulib_container_netdev *d, int up)
{
  if (!d || !d->queue_capacity) return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  d->up = up != 0;
  return 0;
}
int mulib_container_netdev_inject (mulib_container_netdev *d, const void *p, size_t n)
{
  int rc;
  if (!d || !d->queue_capacity) return MULIB_CT_EINVAL;
  if (!d->up) return MULIB_CT_ESTATE;
  rc = mulib_container_netdev_packet_ (d, p, n);
  if (rc) return rc;
  if (d->count == d->queue_capacity || n > d->max_queued_bytes - d->queued_bytes)
    { d->dropped = mulib_ct_counter_add (d->dropped, 1); return MULIB_CT_AGAIN; }
  rc = mulib_ct_blob_assign (d->queue + (d->head + d->count) % d->queue_capacity,
                              p, n, d->mtu + 14);
  if (rc) return rc;
  ++d->count; d->queued_bytes += n;
  d->rx_packets = mulib_ct_counter_add (d->rx_packets, 1);
  d->rx_bytes = mulib_ct_counter_add (d->rx_bytes, n);
  return 0;
}
int
mulib_container_netdev_receive (mulib_container_netdev *d, void *p, size_t cap, size_t *n)
{
  mulib_ct_blob *b;
  if (n) *n = 0;
  if (!d || !d->queue_capacity || !n || (!p && cap)) return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  if (!d->up) return MULIB_CT_ESTATE;
  if (!d->count) return MULIB_CT_AGAIN;
  b = d->queue + d->head; *n = b->length;
  if (b->length > cap) return MULIB_CT_ELIMIT;
  memcpy (p, b->data, b->length); d->queued_bytes -= b->length;
  mulib_ct_blob_free (b); d->head = (d->head + 1) % d->queue_capacity; --d->count;
  return 0;
}
int mulib_container_netdev_send (mulib_container_netdev *d, const void *p, size_t n)
{
  int rc;
  if (!d || !d->queue_capacity) return MULIB_CT_EINVAL;
  if (d->busy) return MULIB_CT_EBUSY;
  if (!d->up) return MULIB_CT_ESTATE;
  if (!d->transmit) return MULIB_CT_ENOTSUP;
  rc = mulib_container_netdev_packet_ (d, p, n);
  if (rc) return rc;
  d->busy = 1; rc = d->transmit (d->user, p, n); d->busy = 0;
  if (rc > 0 && rc != MULIB_CT_AGAIN) rc = MULIB_CT_EIO;
  if (!rc)
    {
      d->tx_packets = mulib_ct_counter_add (d->tx_packets, 1);
      d->tx_bytes = mulib_ct_counter_add (d->tx_bytes, n);
    }
  return rc;
}
int mulib_container_netdev_peer (void *peer, const void *p, size_t n)
{ return mulib_container_netdev_inject ((mulib_container_netdev *)peer, p, n); }
#endif
