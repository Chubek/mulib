#ifndef ON1X_MULIB_H
#define ON1X_MULIB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct on1x_mulib_kernel { const char *name, *header, *sha256; };
struct on1x_mulib_fixture { const char *name, *group, *sha256; const unsigned char *data; size_t size; bool text; };
int64_t on1x_mulib_abi_version(void);
int64_t on1x_mulib_pointer_bits(void);
int64_t on1x_mulib_max_size(void);
int64_t on1x_mulib_kernel_count(void);
const char *on1x_mulib_kernel_name(int64_t index);
bool on1x_mulib_has_kernel(const char *name);
const char *on1x_mulib_kernel_header(const char *name);
const char *on1x_mulib_kernel_sha256(const char *name);
int64_t on1x_mulib_fixture_count(void);
const char *on1x_mulib_fixture_name(int64_t index);
bool on1x_mulib_has_fixture(const char *name);
bool on1x_mulib_fixture_is_text(const char *name);
const char *on1x_mulib_fixture_text(const char *name);
const char *on1x_mulib_fixture_group(const char *name);
const char *on1x_mulib_fixture_sha256(const char *name);
int64_t on1x_mulib_fixture_size(const char *name);
int64_t on1x_mulib_fixture_byte(const char *name,int64_t index);
#endif
