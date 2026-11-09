#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PDF
#include "mulib-pdf.h"
#include <assert.h>
int
main (void)
{
  mulib_pdf_t document = { 0 };
  mulib_buffer_t bytes = { 0 };
  size_t page;
  assert (mulib_pdf_add_page (&document, 595, 842, &page) == 0);
  assert (mulib_pdf_text (&document, page, 72, 770, 18, "Hello PDF!", 10)
          == 0);
  assert (mulib_pdf_color (&document, page, 0.1, 0.3, 0.7) == 0);
  assert (mulib_pdf_rect (&document, page, 72, 700, 200, 30, 0, 1) == 0);
  assert (mulib_pdf_write (&document, &bytes) == 0);
  assert (bytes.length > 8 && !memcmp (bytes.data, "%PDF-1.4", 8));
  mulib_pdf_free (&document);
  mulib_buffer_free (&bytes);
  return 0;
}
