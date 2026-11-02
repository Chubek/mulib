#define MULIB_IMPL_ARTIFACT
#include "mulib-artifact.h"
#include <assert.h>
#include <string.h>
int main (void) {
  const unsigned char bytes[] = { 'a', 0, 'b' };
  mulib_artifact_desc_t desc = {0};
  mulib_artifact_t *value;
  desc.id = "message:1";
  desc.type = "agent.message";
  desc.media_type = "application/octet-stream";
  desc.data = bytes; desc.length = sizeof (bytes);
  value = mulib_artifact_create (&desc);
  assert (value && mulib_artifact_retain (value) == value);
  assert (!memcmp (mulib_artifact_view (value)->data, bytes, sizeof (bytes)));
  mulib_artifact_release (value);
  mulib_artifact_release (value);
  return 0;
}
