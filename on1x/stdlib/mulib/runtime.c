/* Included by the generated immutable metadata TU, after its two tables. */
#include <limits.h>
#include <string.h>
int64_t on1x_mulib_abi_version(void) { return 1; }
int64_t on1x_mulib_pointer_bits(void) { return (int64_t)(sizeof(void *)*CHAR_BIT); }
int64_t on1x_mulib_max_size(void) { return SIZE_MAX>INT64_MAX?INT64_MAX:(int64_t)SIZE_MAX; }
int64_t on1x_mulib_kernel_count(void) { return (int64_t)(sizeof kernels/sizeof kernels[0])-1; }
const char *on1x_mulib_kernel_name(int64_t i) { return i>=0&&i<on1x_mulib_kernel_count()?kernels[i].name:""; }
static const struct on1x_mulib_kernel *kernel(const char *name) {
  if(name)for(size_t i=0;kernels[i].name;++i)if(!strcmp(kernels[i].name,name))return &kernels[i];
  return NULL;
}
bool on1x_mulib_has_kernel(const char *name) { return kernel(name)!=NULL; }
const char *on1x_mulib_kernel_header(const char *name) { const struct on1x_mulib_kernel *p=kernel(name);return p?p->header:""; }
const char *on1x_mulib_kernel_sha256(const char *name) { const struct on1x_mulib_kernel *p=kernel(name);return p?p->sha256:""; }
int64_t on1x_mulib_fixture_count(void) { return (int64_t)(sizeof fixtures/sizeof fixtures[0])-1; }
const char *on1x_mulib_fixture_name(int64_t i) { return i>=0&&i<on1x_mulib_fixture_count()?fixtures[i].name:""; }
static const struct on1x_mulib_fixture *fixture(const char *name) {
  if(name)for(size_t i=0;fixtures[i].name;++i)if(!strcmp(fixtures[i].name,name))return &fixtures[i];
  return NULL;
}
bool on1x_mulib_has_fixture(const char *name) { return fixture(name)!=NULL; }
bool on1x_mulib_fixture_is_text(const char *name) { const struct on1x_mulib_fixture *p=fixture(name);return p&&p->text; }
const char *on1x_mulib_fixture_text(const char *name) { const struct on1x_mulib_fixture *p=fixture(name);return p&&p->text?(const char *)p->data:""; }
const char *on1x_mulib_fixture_group(const char *name) { const struct on1x_mulib_fixture *p=fixture(name);return p?p->group:""; }
const char *on1x_mulib_fixture_sha256(const char *name) { const struct on1x_mulib_fixture *p=fixture(name);return p?p->sha256:""; }
int64_t on1x_mulib_fixture_size(const char *name) { const struct on1x_mulib_fixture *p=fixture(name);return p?(int64_t)p->size:-1; }
int64_t on1x_mulib_fixture_byte(const char *name,int64_t i) {
  const struct on1x_mulib_fixture *p=fixture(name);return p&&i>=0&&(uint64_t)i<p->size?p->data[i]:-1;
}
