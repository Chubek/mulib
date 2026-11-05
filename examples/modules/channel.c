#define MULIB_IMPL_XTHREADS
#define MULIB_IMPL_FIBER
#define MULIB_IMPL_CHANNEL
#include "mulib-channel.h"
#include <assert.h>
static void producer (void *arg)
{
  mulib_channel_t *channel = (mulib_channel_t *)arg; int value = 42;
  assert (!mulib_channel_send (mulib_channel_sender (channel), &value, MULIB_XWAIT_FOREVER));
  assert (!mulib_channel_close (channel));
}
static void consumer (void *arg)
{
  mulib_channel_t *channel = (mulib_channel_t *)arg;
  int value = 0; size_t selected;
  mulib_channel_case_t cases[2];
  cases[0] = mulib_channel_receive_case (mulib_channel_receiver (channel), &value);
  cases[1] = mulib_channel_receive_case (mulib_channel_receiver (NULL), NULL);
  assert (!mulib_channel_select (cases, 2, 0, MULIB_XWAIT_FOREVER, &selected));
  assert (selected == 0 && value == 42);
  assert (mulib_channel_receive (mulib_channel_receiver (channel), &value,
                                MULIB_XWAIT_FOREVER) == MULIB_CHANNEL_CLOSED);
  assert (value == 0);
}
int main (void)
{
  mulib_channel_t *channel; mulib_fiber_scheduler_t *scheduler;
  assert (!mulib_channel_create (&channel, sizeof (int), 0));
  assert (!mulib_fiber_scheduler_create (&scheduler));
  assert (!mulib_fiber_spawn (scheduler, consumer, channel, 0, NULL));
  assert (!mulib_fiber_spawn (scheduler, producer, channel, 0, NULL));
  assert (!mulib_fiber_run (scheduler));
  assert (!mulib_fiber_scheduler_destroy (scheduler));
  assert (!mulib_channel_destroy (channel)); return 0;
}
