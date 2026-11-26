#include "context.h"

#include <string.h>

enum { PROBE_HEADER_SIZE = 7 };

static const unsigned char probe_magic[] = { 0x50, 0x52, 0x42, 1 };
_Static_assert(sizeof probe_magic + 3 == PROBE_HEADER_SIZE, "probe context header size");

static int probe_key_ok(const unsigned char *key, size_t size)
{
  if (size == 0 || size > BASIS_HTTP_PROBE_KEY_MAX) return 0;
  for (size_t i = 0; i < size; ++i) {
    unsigned char c = key[i];
    if ((c >= 0x41 && c <= 0x5a) || (c >= 0x61 && c <= 0x7a) ||
        (c >= 0x30 && c <= 0x39) || (i && (c == 0x5f || c == 0x2d))) continue;
    return 0;
  }
  return 1;
}

static size_t probe_key_length(const char key[BASIS_HTTP_PROBE_KEY_MAX + 1])
{
  const char *end = memchr(key, '\0', BASIS_HTTP_PROBE_KEY_MAX + 1);
  if (!end || !probe_key_ok((const unsigned char *)key, (size_t)(end - key))) return 0;
  return (size_t)(end - key);
}

basis_http_probe_context_status
basis_http_probe_context_encode(const basis_http_probe_context *context,
                                unsigned char *output, size_t capacity,
                                size_t *length)
{
  if (!context || !output || !length) return BASIS_HTTP_PROBE_CONTEXT_INVALID;
  const char *keys[] = { context->probe, context->runner, context->context };
  size_t sizes[3], total = PROBE_HEADER_SIZE;
  for (size_t i = 0; i < 3; ++i) {
    sizes[i] = probe_key_length(keys[i]);
    if (!sizes[i]) return BASIS_HTTP_PROBE_CONTEXT_INVALID;
    total += sizes[i];
  }
  if (capacity < total) return BASIS_HTTP_PROBE_CONTEXT_CAPACITY;

  memcpy(output, probe_magic, sizeof probe_magic);
  size_t pos = PROBE_HEADER_SIZE;
  for (size_t i = 0; i < 3; ++i) {
    output[sizeof probe_magic + i] = (unsigned char)sizes[i];
    memcpy(output + pos, keys[i], sizes[i]);
    pos += sizes[i];
  }
  *length = total;
  return BASIS_HTTP_PROBE_CONTEXT_OK;
}

basis_http_probe_context_status
basis_http_probe_context_decode(const unsigned char *input, size_t length,
                                basis_http_probe_context *context)
{
  if (!input || !context || length < PROBE_HEADER_SIZE ||
      length > BASIS_HTTP_PROBE_CONTEXT_MAX ||
      memcmp(input, probe_magic, sizeof probe_magic)) return BASIS_HTTP_PROBE_CONTEXT_INVALID;

  basis_http_probe_context decoded = {0};
  char *keys[] = { decoded.probe, decoded.runner, decoded.context };
  size_t pos = PROBE_HEADER_SIZE;
  for (size_t i = 0; i < 3; ++i) {
    size_t size = input[sizeof probe_magic + i];
    if (size > length - pos || !probe_key_ok(input + pos, size))
      return BASIS_HTTP_PROBE_CONTEXT_INVALID;
    memcpy(keys[i], input + pos, size);
    pos += size;
  }
  if (pos != length) return BASIS_HTTP_PROBE_CONTEXT_INVALID;
  *context = decoded;
  return BASIS_HTTP_PROBE_CONTEXT_OK;
}
