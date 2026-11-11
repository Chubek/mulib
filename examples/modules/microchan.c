#include "../../library/microchan/runtime.c"
#include <assert.h>
#include <stdio.h>
int main (void)
{
  mulib_microchan_message_t message = {0}, parsed;
  mulib_ct_blob bytes = {0};
  size_t consumed;
  message.opcode = MULIB_MICROCHAN_PUBLISH; message.id = 42;
  strcpy (message.topic, "muw3/demo/events");
  message.data = (const unsigned char *)"hello\0world"; message.length = 11;
  assert (!mulib_microchan_encode (&message, 0, &bytes));
  assert (!mulib_microchan_decode (bytes.data, bytes.length, 0, &parsed, &consumed));
  printf ("%s: %zu payload bytes, correlation %llu\n", parsed.topic, parsed.length,
      (unsigned long long)parsed.id);
  mulib_ct_blob_free (&bytes); return 0;
}
