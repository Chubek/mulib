#include "on1x_kernel_package.h"

#include <mulib-sha.h>
#include <stdio.h>
#include <string.h>

static const unsigned char package_magic[] = "ON1XKP\r\n";
static const char hex_digits[] = "0123456789abcdef";

enum { DIGEST_BYTES = 32, FIXED_HEADER_BYTES = 8 + 5 + 3 * 4 };

static int lower(unsigned char c) { return c >= 'a' && c <= 'z'; }

static int name_valid(const char *name, int module) {
  int component_start = 1, saw_dot = 0;
  if (!name)
    return 0;
  for (size_t i = 0; i <= ON1X_KERNEL_PACKAGE_NAME_MAX; ++i) {
    const unsigned char c = (unsigned char)name[i];
    if (c == '\0')
      return !component_start && (!module || saw_dot);
    if (component_start) {
      if (!lower(c))
        return 0;
      component_start = 0;
    } else if (module && c == '.') {
      saw_dot = 1;
      component_start = 1;
    } else if (!lower(c) && !(c >= '0' && c <= '9') && c != '_') {
      return 0;
    }
  }
  return 0;
}

int on1x_kernel_package_module_valid(const char *module) {
  return name_valid(module, 1);
}

static int hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

static int digest_valid(const char *digest) {
  if (!digest)
    return 0;
  for (size_t i = 0; i < ON1X_KERNEL_SHA256_HEX_LEN; ++i)
    if (hex_value(digest[i]) < 0)
      return 0;
  return digest[ON1X_KERNEL_SHA256_HEX_LEN] == '\0';
}

static void digest_to_hex(const unsigned char raw[DIGEST_BYTES],
                          char hex[ON1X_KERNEL_SHA256_HEX_LEN + 1u]) {
  for (size_t i = 0; i < DIGEST_BYTES; ++i) {
    hex[2u * i] = hex_digits[raw[i] >> 4u];
    hex[2u * i + 1u] = hex_digits[raw[i] & 15u];
  }
  hex[ON1X_KERNEL_SHA256_HEX_LEN] = '\0';
}

int on1x_kernel_import_valid(const struct on1x_kernel_import *record) {
  return record && record->version && name_valid(record->module, 1) &&
         name_valid(record->export_name, 0) &&
         (record->arity == 2 || record->arity == 3) &&
         digest_valid(record->interface_sha256) &&
         digest_valid(record->behavior_sha256);
}

int on1x_kernel_hash_interface(const char *module, const char *export_name,
                              unsigned arity,
                              char out[ON1X_KERNEL_SHA256_HEX_LEN + 1u]) {
  char preimage[2u * ON1X_KERNEL_PACKAGE_NAME_MAX + 80u];
  unsigned char raw[DIGEST_BYTES];
  int length;
  if (!out || !name_valid(module, 1) || !name_valid(export_name, 0) ||
      (arity != 2 && arity != 3))
    return 0;
  length = snprintf(preimage, sizeof(preimage),
                    "on1x.kernel.interface/v1\nmodule=%s\nexport=%s\n"
                    "signature=(i64,i64%s)->i64\n",
                    module, export_name, arity == 3 ? ",i64" : "");
  if (length < 0 || (size_t)length >= sizeof(preimage) ||
      !mulib_sha256(preimage, (size_t)length, raw))
    return 0;
  digest_to_hex(raw, out);
  return 1;
}

/* This also computes the unique encoded length once all fields are bounded. */
static int manifest_size(const struct on1x_kernel_package *p, size_t *length) {
  size_t size = FIXED_HEADER_BYTES;
  if (!p || !name_valid(p->module, 1) || !p->version || !p->export_count ||
      p->export_count > ON1X_KERNEL_PACKAGE_EXPORT_MAX ||
      p->dep_count > ON1X_KERNEL_PACKAGE_DEP_MAX ||
      p->conflict_count > ON1X_KERNEL_PACKAGE_CONFLICT_MAX ||
      !p->recipe || !p->recipe_size ||
      p->recipe_size > ON1X_KERNEL_PACKAGE_RECIPE_MAX)
    return 0;

  switch (p->kind) {
  case ON1X_KERNEL_PACKAGE_RECIPE:
    if (p->image_size)
      return 0;
    break;
  case ON1X_KERNEL_PACKAGE_NATIVE_AMD64_LINUX_V1:
    if (!p->image || !p->image_size ||
        p->image_size > ON1X_KERNEL_PACKAGE_IMAGE_MAX)
      return 0;
    size += DIGEST_BYTES;
    break;
  default:
    return 0;
  }

  size += 1u + strlen(p->module);
  for (size_t i = 0; i < p->export_count; ++i) {
    const struct on1x_kernel_package_export *e = &p->exports[i];
    char expected[ON1X_KERNEL_SHA256_HEX_LEN + 1u];
    if (!on1x_kernel_hash_interface(p->module, e->name, e->arity, expected) ||
        (i && strcmp(p->exports[i - 1u].name, e->name) >= 0) ||
        memcmp(expected, e->interface_sha256, sizeof(expected)) != 0 ||
        !digest_valid(e->behavior_sha256))
      return 0;
    size += 1u + strlen(e->name) + 1u + 2u * DIGEST_BYTES;
  }
  for (size_t i = 0; i < p->dep_count; ++i) {
    const struct on1x_kernel_package_dep *d = &p->deps[i];
    if (!name_valid(d->module, 1) || !d->version ||
        strcmp(p->module, d->module) == 0 ||
        (i && strcmp(p->deps[i - 1u].module, d->module) >= 0))
      return 0;
    size += 1u + strlen(d->module) + 4u;
  }
  for (size_t i = 0; i < p->conflict_count; ++i) {
    const char *module = p->conflicts[i].module;
    if (!name_valid(module, 1) || strcmp(p->module, module) == 0 ||
        (i && strcmp(p->conflicts[i - 1u].module, module) >= 0))
      return 0;
    for (size_t j = 0; j < p->dep_count; ++j)
      if (strcmp(p->deps[j].module, module) == 0)
        return 0;
    size += 1u + strlen(module);
  }
  *length = size + p->recipe_size + p->image_size;
  return *length <= ON1X_KERNEL_PACKAGE_SIZE_MAX;
}

struct reader {
  const unsigned char *next;
  size_t remaining;
};

static const unsigned char *take(struct reader *r, size_t n) {
  const unsigned char *p = r->next;
  if (n > r->remaining)
    return NULL;
  r->next += n;
  r->remaining -= n;
  return p;
}

static int read_byte(struct reader *r, unsigned *value) {
  const unsigned char *p = take(r, 1);
  if (!p)
    return 0;
  *value = *p;
  return 1;
}

static int read_u32(struct reader *r, uint32_t *value) {
  const unsigned char *p = take(r, 4);
  if (!p)
    return 0;
  *value = ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) | p[3];
  return 1;
}

static int read_name(struct reader *r, char *out) {
  unsigned n;
  const unsigned char *p;
  if (!read_byte(r, &n) || !n || n > ON1X_KERNEL_PACKAGE_NAME_MAX ||
      !(p = take(r, n)) || memchr(p, '\0', n))
    return 0;
  memcpy(out, p, n);
  out[n] = '\0';
  return 1;
}

static int read_digest(struct reader *r, char *out) {
  const unsigned char *p = take(r, DIGEST_BYTES);
  if (!p)
    return 0;
  digest_to_hex(p, out);
  return 1;
}

int on1x_kernel_package_parse(const void *data, size_t size,
                             struct on1x_kernel_package *out) {
  struct on1x_kernel_package package = {0};
  struct reader r = {data, size};
  unsigned format, kind, exports, deps, conflicts;
  uint32_t recipe_size, image_size;
  const unsigned char *magic, *image_digest = NULL;
  size_t expected_size;
  if (!data || !out || size > ON1X_KERNEL_PACKAGE_SIZE_MAX ||
      !(magic = take(&r, sizeof(package_magic) - 1u)) ||
      memcmp(magic, package_magic, sizeof(package_magic) - 1u) != 0 ||
      !read_byte(&r, &format) || format != ON1X_KERNEL_PACKAGE_FORMAT ||
      !read_byte(&r, &kind) ||
      (kind != ON1X_KERNEL_PACKAGE_RECIPE &&
       kind != ON1X_KERNEL_PACKAGE_NATIVE_AMD64_LINUX_V1) ||
      !read_byte(&r, &exports) || !exports ||
      exports > ON1X_KERNEL_PACKAGE_EXPORT_MAX ||
      !read_byte(&r, &deps) || deps > ON1X_KERNEL_PACKAGE_DEP_MAX ||
      !read_byte(&r, &conflicts) || conflicts > ON1X_KERNEL_PACKAGE_CONFLICT_MAX ||
      !read_u32(&r, &package.version) || !package.version ||
      !read_u32(&r, &recipe_size) || !recipe_size ||
      recipe_size > ON1X_KERNEL_PACKAGE_RECIPE_MAX ||
      !read_u32(&r, &image_size) || image_size > ON1X_KERNEL_PACKAGE_IMAGE_MAX ||
      ((kind == ON1X_KERNEL_PACKAGE_RECIPE) != (image_size == 0)) ||
      !read_name(&r, package.module))
    return 0;

  package.kind = (enum on1x_kernel_package_kind)kind;
  package.export_count = exports;
  package.dep_count = deps;
  package.conflict_count = conflicts;
  package.recipe_size = recipe_size;
  package.image_size = image_size;
  for (size_t i = 0; i < exports; ++i) {
    struct on1x_kernel_package_export *e = &package.exports[i];
    if (!read_name(&r, e->name) || !read_byte(&r, &e->arity) ||
        !read_digest(&r, e->interface_sha256) ||
        !read_digest(&r, e->behavior_sha256))
      return 0;
  }
  for (size_t i = 0; i < deps; ++i)
    if (!read_name(&r, package.deps[i].module) ||
        !read_u32(&r, &package.deps[i].version))
      return 0;
  for (size_t i = 0; i < conflicts; ++i)
    if (!read_name(&r, package.conflicts[i].module))
      return 0;

  if (image_size && !(image_digest = take(&r, DIGEST_BYTES)))
    return 0;
  package.recipe = take(&r, recipe_size);
  if (image_size)
    package.image = take(&r, image_size);
  if (r.remaining || !manifest_size(&package, &expected_size) ||
      expected_size != size)
    return 0;
  if (image_digest) {
    unsigned char raw[DIGEST_BYTES];
    if (!mulib_sha256(package.image, image_size, raw) ||
        memcmp(raw, image_digest, sizeof(raw)) != 0)
      return 0;
    digest_to_hex(raw, package.image_sha256);
  }
  *out = package;
  return 1;
}

static unsigned char *write_u32(unsigned char *p, uint32_t value) {
  *p++ = (unsigned char)(value >> 24u);
  *p++ = (unsigned char)(value >> 16u);
  *p++ = (unsigned char)(value >> 8u);
  *p++ = (unsigned char)value;
  return p;
}

static unsigned char *write_name(unsigned char *p, const char *name) {
  const size_t n = strlen(name);
  *p++ = (unsigned char)n;
  memcpy(p, name, n);
  return p + n;
}

static unsigned char *write_digest(unsigned char *p, const char *hex) {
  for (size_t i = 0; i < DIGEST_BYTES; ++i)
    *p++ = (unsigned char)((hex_value(hex[2u * i]) << 4u) |
                          hex_value(hex[2u * i + 1u]));
  return p;
}

int on1x_kernel_package_encode(const struct on1x_kernel_package *package,
                              void *data, size_t capacity, size_t *written) {
  size_t size;
  unsigned char image_digest[DIGEST_BYTES], *p = data;
  if (!written || !manifest_size(package, &size))
    return 0;
  if (!data) {
    if (capacity)
      return 0;
    *written = size;
    return 1;
  }
  if (capacity < size ||
      (package->image_size &&
       !mulib_sha256(package->image, package->image_size, image_digest)))
    return 0;
  memcpy(p, package_magic, sizeof(package_magic) - 1u);
  p += sizeof(package_magic) - 1u;
  *p++ = ON1X_KERNEL_PACKAGE_FORMAT;
  *p++ = (unsigned char)package->kind;
  *p++ = (unsigned char)package->export_count;
  *p++ = (unsigned char)package->dep_count;
  *p++ = (unsigned char)package->conflict_count;
  p = write_u32(p, package->version);
  p = write_u32(p, (uint32_t)package->recipe_size);
  p = write_u32(p, (uint32_t)package->image_size);
  p = write_name(p, package->module);
  for (size_t i = 0; i < package->export_count; ++i) {
    const struct on1x_kernel_package_export *e = &package->exports[i];
    p = write_name(p, e->name);
    *p++ = (unsigned char)e->arity;
    p = write_digest(p, e->interface_sha256);
    p = write_digest(p, e->behavior_sha256);
  }
  for (size_t i = 0; i < package->dep_count; ++i) {
    p = write_name(p, package->deps[i].module);
    p = write_u32(p, package->deps[i].version);
  }
  for (size_t i = 0; i < package->conflict_count; ++i)
    p = write_name(p, package->conflicts[i].module);
  if (package->image_size) {
    memcpy(p, image_digest, sizeof(image_digest));
    p += sizeof(image_digest);
  }
  memcpy(p, package->recipe, package->recipe_size);
  if (package->image_size)
    memcpy(p + package->recipe_size, package->image, package->image_size);
  *written = size;
  return 1;
}
