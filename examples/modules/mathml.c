#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_XML
#define MULIB_IMPL_MATHML
#include "mulib-mathml.h"
#include <assert.h>
int
main (void)
{
  mulib_xml_node_t *math = mulib_mathml_new (0),
                   *fraction = mulib_mathml_element ("mfrac");
  mulib_xml_node_t *one = mulib_mathml_token ("mn", "1"),
                   *x = mulib_mathml_token ("mi", "x");
  mulib_xml_node_t *parsed;
  char *xml;
  size_t length;
  assert (math && fraction && one && x);
  assert (mulib_xml_append (fraction, one) && mulib_xml_append (fraction, x));
  assert (mulib_xml_append (math, fraction));
  xml = mulib_mathml_serialize (math, &length, NULL);
  assert (xml);
  parsed = mulib_mathml_parse (xml, length, NULL);
  assert (parsed);
  mulib_xml_destroy (parsed);
  free (xml);
  mulib_xml_destroy (math);
  return 0;
}
