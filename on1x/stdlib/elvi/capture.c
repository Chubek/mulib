#include "capture.h"
#include <limits.h>

mulib_elvi_script_capture_result_t on1x_elvi_capture_https(
    const mulib_elvi_scrape_program_t *program,
    const mulib_elvi_script_capture_config_t *config,
    mulib_elvi_script_document_t *out)
{
  return mulib_elvi_script_capture_https(program, config, out);
}

int on1x_elvi_document_copy_hex(const mulib_elvi_script_document_t *document,
    char *out, size_t capacity)
{
  static const char digits[] = "0123456789abcdef";
  if (!document || !out || !capacity ||
      document->body_length > MULIB_ELVI_SCRIPT_MAX_BODY ||
      (document->body_length && !document->body) ||
      document->body_length > (capacity - 1) / 2) return 0;
#if UCHAR_MAX > 255
  for (size_t i = 0; i < document->body_length; ++i)
    if (document->body[i] > 255) return 0;
#endif
  for (size_t i = 0; i < document->body_length; ++i) {
    unsigned byte = document->body[i];
    out[2 * i] = digits[byte >> 4];
    out[2 * i + 1] = digits[byte & 15];
  }
  out[2 * document->body_length] = '\0';
  return 1;
}
