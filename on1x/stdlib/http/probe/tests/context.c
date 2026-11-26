#include "../context.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void invalid(const unsigned char *bytes, size_t length)
{
  basis_http_probe_context decoded = { "sentinel", "unchanged", "before" };
  basis_http_probe_context before = decoded;
  assert(basis_http_probe_context_decode(bytes, length, &decoded) ==
         BASIS_HTTP_PROBE_CONTEXT_INVALID);
  assert(memcmp(decoded.probe, before.probe, sizeof decoded.probe) == 0);
  assert(memcmp(decoded.runner, before.runner, sizeof decoded.runner) == 0);
  assert(memcmp(decoded.context, before.context, sizeof decoded.context) == 0);
}

int main(void)
{
  const basis_http_probe_context original = { "p", "runner-1", "context_9" };
  const unsigned char expected[] = "PRB\x01\x01\x08\x09" "prunner-1context_9";
  unsigned char output[BASIS_HTTP_PROBE_CONTEXT_MAX + 1];
  memset(output, 0xff, sizeof output);
  size_t length = SIZE_MAX;
  assert(basis_http_probe_context_encode(&original, output, 7, &length) ==
         BASIS_HTTP_PROBE_CONTEXT_CAPACITY);
  assert(length == SIZE_MAX);
  for (size_t i = 0; i < sizeof output; ++i) assert(output[i] == 0xff);
  assert(basis_http_probe_context_encode(&original, output, sizeof output, &length) ==
         BASIS_HTTP_PROBE_CONTEXT_OK);
  assert(length == sizeof expected - 1);
  assert(memcmp(output, expected, length) == 0);
  assert(output[length] == 0xff);

  basis_http_probe_context decoded;
  assert(basis_http_probe_context_decode(output, length, &decoded) ==
         BASIS_HTTP_PROBE_CONTEXT_OK);
  assert(strcmp(decoded.probe, original.probe) == 0);
  assert(strcmp(decoded.runner, original.runner) == 0);
  assert(strcmp(decoded.context, original.context) == 0);

  invalid(NULL, length);
  invalid(output, SIZE_MAX);
  invalid(output, length + 1);
  for (size_t i = 0; i < length; ++i) invalid(output, i);
  for (size_t i = 0; i < 7; ++i) {
    unsigned char v = output[i];
    output[i] = 0xff;
    invalid(output, length);
    output[i] = v;
  }
  for (size_t i = 7; i < length; ++i) {
    unsigned char v = output[i];
    output[i] = 0;
    invalid(output, length);
    output[i] = '/';
    invalid(output, length);
    output[i] = v;
  }

  basis_http_probe_context maximal;
  memset(maximal.probe, 'A', sizeof maximal.probe);
  memset(maximal.runner, 'b', sizeof maximal.runner);
  memset(maximal.context, '9', sizeof maximal.context);
  length = SIZE_MAX;
  assert(basis_http_probe_context_encode(&maximal, output, sizeof output, &length) ==
         BASIS_HTTP_PROBE_CONTEXT_INVALID);
  assert(length == SIZE_MAX);
  maximal.probe[BASIS_HTTP_PROBE_KEY_MAX] = '\0';
  maximal.runner[BASIS_HTTP_PROBE_KEY_MAX] = '\0';
  maximal.context[BASIS_HTTP_PROBE_KEY_MAX] = '\0';
  assert(basis_http_probe_context_encode(&maximal, output, sizeof output, &length) ==
         BASIS_HTTP_PROBE_CONTEXT_OK);
  assert(length == BASIS_HTTP_PROBE_CONTEXT_MAX);
  assert(basis_http_probe_context_decode(output, length, &decoded) ==
         BASIS_HTTP_PROBE_CONTEXT_OK);
  assert(strcmp(decoded.probe, maximal.probe) == 0);
  assert(strcmp(decoded.runner, maximal.runner) == 0);
  assert(strcmp(decoded.context, maximal.context) == 0);
  return 0;
}
