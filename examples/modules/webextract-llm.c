/* Complete, offline webextract-llm example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_WEBEXTRACT_LLM
#include "mulib-webextract-llm.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
reply (void *ctx, const char *request, size_t length,
       mulib_shylock_buffer_t *response)
{
  (void)ctx;
  assert (request && length);
  return mulib_shylock_text (response, "{}");
}

int
main (void)
{
  mulib_shylock_buffer_t request = { 0 }, response = { 0 };
  assert (mulib_webextract_llm_request (
      "model", "Price: 12", "{\"type\":\"object\"}", 100, &request));
  assert (mulib_webextract_llm_call (reply, NULL, request.data, request.len,
                                     &response));
  mulib_shylock_buffer_free (&request);
  mulib_shylock_buffer_free (&response);
  return 0;
}
