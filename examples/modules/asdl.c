#define MULIB_IMPL_ASDL
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#include "mulib-asdl.h"
#include <assert.h>

int main(void) {
  static const mulib_asdl_field_t fields[] = {MULIB_ASDL_FIELD("int", "value")};
  static const mulib_asdl_constructor_t constructors[] = {
    MULIB_ASDL_CONSTRUCTOR("Num", fields), MULIB_ASDL_ATOM("Empty")
  };
  static const mulib_asdl_type_t types[] = {MULIB_ASDL_SUM_TYPE("expr", constructors)};
  const char source[] = "module Example { expr = Num(int value) | Empty }";
  mulib_asdl_error_t error;
  mulib_asdl_options_t options = {0};
  mulib_strbuf_t parsed = {0}, declared = {0};
  mulib_asdl_module_t *module = mulib_asdl_from_types("Example", types, MULIB_ASDL_COUNT(types), &error);
  assert(module);
  assert(mulib_asdl_compile(source, sizeof source - 1, NULL, &parsed, &error));
  assert(mulib_asdl_generate(module, NULL, &declared, &error));
  assert(!strcmp(parsed.data, declared.data));
  puts(parsed.data);
  mulib_strbuf_free(&parsed); mulib_strbuf_free(&declared);
  options.format = MULIB_ASDL_C99; options.c_prefix = "example";
  assert(mulib_asdl_generate(module, &options, &declared, &error));
  assert(strstr(declared.data, "example_make_expr_Num"));
  mulib_strbuf_free(&declared); mulib_asdl_destroy(module); return 0;
}
