#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "on1x_kernel_native.h"

#if defined(ON1X_KERNEL_NATIVE_QBFD) && ON1X_KERNEL_NATIVE_QBFD && \
    defined(__linux__) && defined(__x86_64__) && !defined(__ILP32__)
#include <features.h>
#if defined(__GLIBC__)
#define NATIVE_ENABLED 1
#endif
#endif

#ifdef NATIVE_ENABLED
#include "on1x_sealed_native.h"
#include <BFDBinABI.h>
#include <mulib-sha.h>

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef PT_GNU_PROPERTY
#define PT_GNU_PROPERTY 0x6474e553u
#endif
#ifndef SHT_RELR
#define SHT_RELR 19u
#endif

enum { EXPORT_COUNT = 3, PAGE_SIZE = 4096, PH_LIMIT = 32, SH_LIMIT = 256,
       VADDR_LIMIT = 16 * 1024 * 1024 };

static const struct {
  const char *name, *symbol;
  unsigned arity;
} exports[EXPORT_COUNT] = {
    {"add_i64", "on1x_kernel_native_add_i64", 2},
    {"i64", "on1x_kernel_native_i64", 3},
    {"sub_i64", "on1x_kernel_native_sub_i64", 2},
};

struct on1x_kernel_native {
  void *handle;
  int fd;
  struct on1x_kernel_native_adapter adapters[EXPORT_COUNT];
};

struct elf_image {
  Elf64_Ehdr header;
  Elf64_Phdr ph[PH_LIMIT];
  size_t dynamic_index;
  size_t dynamic_offset, dynsym_offset, dynstr_offset, hash_offset;
  size_t dynstr_size, hash_size;
  Elf64_Sym symbols[EXPORT_COUNT + 1];
  size_t slot_for_export[EXPORT_COUNT];
};

static size_t export_index(const char *name, unsigned arity) {
  if (name)
    for (size_t i = 0; i < EXPORT_COUNT; ++i)
      if (arity == exports[i].arity && !strcmp(name, exports[i].name))
        return i;
  return EXPORT_COUNT;
}

static int hex_digest(const char *s) {
  if (!s) return 0;
  for (size_t i = 0; i < 64; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
      return 0;
  return s[64] == '\0';
}

static int host_approves(const unsigned char *image, size_t image_size,
    const void *policy, size_t policy_size,
    const struct on1x_kernel_native_binding *bindings, size_t binding_count,
    char image_hex[65]) {
  static const char hex[] = "0123456789abcdef";
  const struct on1x_kernel_native_binding *ordered[EXPORT_COUNT] = {0};
  unsigned char digest[32];
  char expected[ON1X_KERNEL_NATIVE_POLICY_LIMIT];
  if (!bindings || binding_count != EXPORT_COUNT ||
      !mulib_sha256(image, image_size, digest)) return 0;
  for (size_t i = 0; i < binding_count; ++i) {
    const struct on1x_kernel_native_binding *b = &bindings[i];
    size_t j = export_index(b->export_name, b->arity);
    if (j == EXPORT_COUNT || ordered[j] ||
        !hex_digest(b->interface_sha256) || !hex_digest(b->behavior_sha256))
      return 0;
    ordered[j] = b;
  }
  for (size_t i = 0; i < sizeof digest; ++i) {
    image_hex[2 * i] = hex[digest[i] >> 4];
    image_hex[2 * i + 1] = hex[digest[i] & 15u];
  }
  image_hex[64] = '\0';
  int n = snprintf(expected, sizeof expected,
      "on1x-native-policy-v1\n"
      "x86_64-unknown-linux-gnu sysv-clamp-v1 stdffi.clamp 1 %s\n", image_hex);
  if (n < 0 || (size_t)n >= sizeof expected) return 0;
  size_t used = (size_t)n;
  for (size_t i = 0; i < EXPORT_COUNT; ++i) {
    n = snprintf(expected + used, sizeof expected - used, "%s %u %s %s %s\n",
        exports[i].name, exports[i].arity, exports[i].symbol,
        ordered[i]->interface_sha256, ordered[i]->behavior_sha256);
    if (n < 0 || (size_t)n >= sizeof expected - used) return 0;
    used += (size_t)n;
  }
  return used == policy_size && !memcmp(expected, policy, used);
}

static int range(uint64_t off, uint64_t len, uint64_t cap) {
  return off <= cap && len <= cap - off;
}

/* Match the loader's file-backed view, not a section's claimed VMA. */
static int file_offset(const struct elf_image *elf, uint64_t va, uint64_t len,
                       uint32_t flags, size_t *out) {
  if (!len) return 0;
  for (size_t i = 0; i < elf->header.e_phnum; ++i) {
    const Elf64_Phdr *p = &elf->ph[i];
    if (p->p_type == PT_LOAD && p->p_flags == flags && va >= p->p_vaddr &&
        range(va - p->p_vaddr, len, p->p_filesz)) {
      *out = (size_t)(p->p_offset + (va - p->p_vaddr));
      return 1;
    }
  }
  return 0;
}

static uint32_t word32(const unsigned char *p) {
  uint32_t n;
  memcpy(&n, p, sizeof n);
  return n;
}

/* Recent GNU tools emit informational "feature used: x86" and
 * "ISA used: x86-64-baseline" properties even without CET or extension
 * options. Do not allow properties requesting other loader behavior. */
static int baseline_gnu_property(const unsigned char *note, uint64_t size) {
  if (size < 32 || size > 48 || (size - 16) % 16 ||
      word32(note) != 4 || word32(note + 4) != size - 16 ||
      word32(note + 8) != 5 /* NT_GNU_PROPERTY_TYPE_0 */ ||
      memcmp(note + 12, "GNU", 4)) return 0;
  uint32_t previous = 0;
  for (size_t i = 16; i < size; i += 16) {
    uint32_t type = word32(note + i);
    if ((type != UINT32_C(0xc0010001) && type != UINT32_C(0xc0010002)) ||
        type <= previous || word32(note + i + 4) != 4 ||
        word32(note + i + 8) != 1 || word32(note + i + 12)) return 0;
    previous = type;
  }
  return 1;
}

static int program_headers(const unsigned char *image, size_t size,
                           struct elf_image *elf) {
  if (size < sizeof elf->header) return 0;
  memcpy(&elf->header, image, sizeof elf->header);
  const Elf64_Ehdr *e = &elf->header;
  if (memcmp(e->e_ident, ELFMAG, SELFMAG) ||
      e->e_ident[EI_CLASS] != ELFCLASS64 || e->e_ident[EI_DATA] != ELFDATA2LSB ||
      e->e_ident[EI_VERSION] != EV_CURRENT ||
      (e->e_ident[EI_OSABI] != ELFOSABI_NONE && e->e_ident[EI_OSABI] != ELFOSABI_GNU) ||
      e->e_ident[EI_ABIVERSION] || e->e_type != ET_DYN || e->e_machine != EM_X86_64 ||
      e->e_version != EV_CURRENT || e->e_entry || e->e_flags ||
      e->e_ehsize != sizeof *e || e->e_phentsize != sizeof(Elf64_Phdr) ||
      !e->e_phnum || e->e_phnum > PH_LIMIT || e->e_phoff != sizeof *e ||
      !range(e->e_phoff, e->e_phnum * sizeof(Elf64_Phdr), size) ||
      e->e_shentsize != sizeof(Elf64_Shdr) || !e->e_shnum || e->e_shnum > SH_LIMIT ||
      e->e_shstrndx == SHN_UNDEF || e->e_shstrndx >= e->e_shnum ||
      e->e_shoff < e->e_phoff + e->e_phnum * sizeof(Elf64_Phdr) ||
      !range(e->e_shoff, e->e_shnum * sizeof(Elf64_Shdr), size)) return 0;
  memcpy(elf->ph, image + e->e_phoff, e->e_phnum * sizeof(Elf64_Phdr));

  uint64_t prev_end = 0;
  int stack = 0, load = 0, executable = 0, property = 0;
  elf->dynamic_index = PH_LIMIT;
  for (size_t i = 0; i < e->e_phnum; ++i) {
    const Elf64_Phdr *p = &elf->ph[i];
    if (!range(p->p_offset, p->p_filesz, size) ||
        !range(p->p_vaddr, p->p_memsz, VADDR_LIMIT) || p->p_filesz > p->p_memsz)
      return 0;
    switch (p->p_type) {
    case PT_LOAD:
      if ((p->p_flags != PF_R && p->p_flags != (PF_R | PF_W) &&
           p->p_flags != (PF_R | PF_X)) || !p->p_memsz ||
          p->p_align != PAGE_SIZE ||
          p->p_offset % PAGE_SIZE != p->p_vaddr % PAGE_SIZE ||
          p->p_vaddr / PAGE_SIZE < prev_end) return 0;
      if (!load && (p->p_offset || p->p_vaddr || p->p_flags != PF_R ||
          p->p_filesz < e->e_phoff + e->e_phnum * sizeof(Elf64_Phdr))) return 0;
      ++load;
      if (p->p_flags & PF_X) ++executable;
      prev_end = (p->p_vaddr + p->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
      break;
    case PT_DYNAMIC:
      if (elf->dynamic_index != PH_LIMIT || p->p_flags != (PF_R | PF_W) ||
          !p->p_filesz || p->p_filesz != p->p_memsz ||
          p->p_filesz % sizeof(Elf64_Dyn) || p->p_vaddr % _Alignof(Elf64_Dyn))
        return 0;
      elf->dynamic_index = i;
      break;
    case PT_GNU_STACK:
      if (stack++ || p->p_flags != (PF_R | PF_W) || p->p_memsz) return 0;
      break;
    case PT_GNU_PROPERTY:
      if (property++ || p->p_flags != PF_R || p->p_filesz != p->p_memsz ||
          p->p_align != 8 || p->p_vaddr % 8 ||
          !baseline_gnu_property(image + p->p_offset, p->p_filesz)) return 0;
      break;
    case PT_NOTE:
    case PT_GNU_EH_FRAME:
    case PT_GNU_RELRO:
      if (p->p_flags != PF_R || p->p_filesz != p->p_memsz) return 0;
      break;
    default:
      /* Includes PT_INTERP, PT_TLS and unsupported loader directives. */
      return 0;
    }
  }
  if (!stack || !load || !executable || elf->dynamic_index == PH_LIMIT)
    return 0;
  for (size_t i = 0; i < e->e_phnum; ++i) {
    const Elf64_Phdr *p = &elf->ph[i];
    if (p->p_type == PT_LOAD || p->p_type == PT_GNU_STACK) continue;
    uint32_t flags = (p->p_type == PT_DYNAMIC || p->p_type == PT_GNU_RELRO) ?
        PF_R | PF_W : PF_R;
    size_t off;
    if (!file_offset(elf, p->p_vaddr, p->p_filesz, flags, &off) ||
        off != p->p_offset) return 0;
  }
  elf->dynamic_offset = (size_t)elf->ph[elf->dynamic_index].p_offset;
  return 1;
}

static uint32_t elf_hash(const unsigned char *s) {
  uint32_t hash = 0;
  while (*s) {
    hash = (hash << 4) + *s++;
    uint32_t high = hash & UINT32_C(0xf0000000);
    hash ^= (high >> 24) ^ high;
  }
  return hash;
}

static int dynamic_tables(const unsigned char *image, struct elf_image *elf) {
  static const Elf64_Sym zero;
  uint64_t hash = 0, strtab = 0, symtab = 0, strsz = 0, syment = 0;
  int ended = 0;
  for (size_t i = 0; i < elf->ph[elf->dynamic_index].p_filesz / sizeof(Elf64_Dyn); ++i) {
    Elf64_Dyn d;
    memcpy(&d, image + elf->dynamic_offset + i * sizeof d, sizeof d);
    if (d.d_tag == DT_NULL) {
      if (d.d_un.d_val) return 0;
      ended = 1;
      continue;
    }
    if (ended || !d.d_un.d_val) return 0;
    uint64_t *value;
    switch (d.d_tag) {
    case DT_HASH: value = &hash; break;
    case DT_STRTAB: value = &strtab; break;
    case DT_SYMTAB: value = &symtab; break;
    case DT_STRSZ: value = &strsz; break;
    case DT_SYMENT: value = &syment; break;
    default:
      /* Positive allow-list: rejects dependencies/search paths, audit tags,
       * hooks/constructors, relocations, alternate hash/symbol/version data,
       * and all unrecognized loader behavior before dlopen. */
      return 0;
    }
    if (*value) return 0;
    *value = d.d_un.d_val;
  }
  if (!ended || !strtab || !symtab || !hash || !strsz ||
      syment != sizeof(Elf64_Sym) || symtab % _Alignof(Elf64_Sym) || hash % 4 ||
      !file_offset(elf, strtab, strsz, PF_R, &elf->dynstr_offset) ||
      !file_offset(elf, symtab, sizeof elf->symbols, PF_R, &elf->dynsym_offset) ||
      !file_offset(elf, hash, 8, PF_R, &elf->hash_offset)) return 0;
  elf->dynstr_size = (size_t)strsz;
  const unsigned char *htable = image + elf->hash_offset;
  uint32_t buckets = word32(htable), chains = word32(htable + 4);
  if (!buckets || buckets > 64 || chains != EXPORT_COUNT + 1) return 0;
  elf->hash_size = (2u + buckets + chains) * 4u;
  if (!file_offset(elf, hash, elf->hash_size, PF_R, &elf->hash_offset))
    return 0;

  const unsigned char *str = image + elf->dynstr_offset;
  memcpy(elf->symbols, image + elf->dynsym_offset, sizeof elf->symbols);
  if (*str || memcmp(&elf->symbols[0], &zero, sizeof zero)) return 0;
  for (size_t i = 1; i < chains; ++i) {
    const Elf64_Sym *s = &elf->symbols[i];
    size_t unused, j;
    if (s->st_name >= strsz ||
        !memchr(str + s->st_name, 0, (size_t)strsz - s->st_name) ||
        s->st_info != ELF64_ST_INFO(STB_GLOBAL, STT_FUNC) ||
        s->st_other != STV_DEFAULT || s->st_shndx == SHN_UNDEF ||
        s->st_shndx >= elf->header.e_shnum ||
        !file_offset(elf, s->st_value, s->st_size, PF_R | PF_X, &unused))
      return 0;
    for (j = 0; j < EXPORT_COUNT; ++j)
      if (!strcmp((const char *)str + s->st_name, exports[j].symbol)) break;
    if (j == EXPORT_COUNT || elf->slot_for_export[j]) return 0;
    for (size_t k = 1; k < i; ++k) {
      const Elf64_Sym *prev = &elf->symbols[k];
      if (prev->st_value < s->st_value + s->st_size &&
          s->st_value < prev->st_value + prev->st_size) return 0;
    }
    elf->slot_for_export[j] = i;
  }
  if (word32(htable + 4 * (2u + buckets))) return 0;
  unsigned seen = 1u;
  for (uint32_t i = 0; i < buckets; ++i) {
    uint32_t slot = word32(htable + 4 * (2u + i));
    while (slot) {
      if (slot >= chains || (seen & (1u << slot)) ||
          elf_hash(str + elf->symbols[slot].st_name) % buckets != i) return 0;
      seen |= 1u << slot;
      slot = word32(htable + 4 * (2u + buckets + slot));
    }
  }
  return seen == (1u << chains) - 1u;
}

/* qBFD supplies the general ELF view; cross-check it against the loader
 * view. qBFD alone is insufficient: section and program metadata need not
 * describe the same tables, and dependencies are only one kind of hook. */
static int qbfd_agrees(const unsigned char *image, size_t size,
                       const struct elf_image *elf) {
  archbfd_object *obj;
  archbfd_object_info info;
  unsigned dynsym = 0, hash = 0, dynamic = 0;
  size_t dynstr_count = 0;
  int ok = 0;
  if (archbfd_abi_version() != ARCHBFD_ABI_VERSION ||
      archbfd_open_memory(image, size, NULL, &obj, NULL) != ARCHBFD_OK)
    return 0;
  if (archbfd_get_object_info(obj, &info, NULL) != ARCHBFD_OK ||
      info.format != ARCHBFD_FORMAT_ELF || info.arch != ARCHBFD_ARCH_X86_64 ||
      info.endian != ARCHBFD_LITTLE_ENDIAN || info.file_type != ARCHBFD_FILE_SHARED ||
      info.address_bits != 64 || info.entry_point || info.image_base ||
      info.file_size != size || info.segment_count != elf->header.e_phnum ||
      info.section_count != elf->header.e_shnum || info.dependency_count ||
      info.dynamic_symbol_count != EXPORT_COUNT + 1)
    goto done;

  for (size_t i = 0; i < info.segment_count; ++i) {
    const Elf64_Phdr *p = &elf->ph[i];
    const uint32_t flags = ((p->p_flags & PF_R) ? ARCHBFD_SEG_READ : 0) |
        ((p->p_flags & PF_W) ? ARCHBFD_SEG_WRITE : 0) |
        ((p->p_flags & PF_X) ? ARCHBFD_SEG_EXEC : 0);
    archbfd_segment s;
    if (archbfd_get_segment(obj, i, &s, NULL) != ARCHBFD_OK ||
        s.native_type != p->p_type || s.flags != flags || s.vaddr != p->p_vaddr ||
        s.file_offset != p->p_offset || s.file_size != p->p_filesz ||
        s.memory_size != p->p_memsz || s.alignment != p->p_align)
      goto done;
  }
  for (size_t i = 0; i < info.section_count; ++i) {
    archbfd_section s;
    size_t relocs;
    if (archbfd_get_section(obj, i, &s, NULL) != ARCHBFD_OK ||
        archbfd_get_relocation_count(obj, i, &relocs, NULL) != ARCHBFD_OK || relocs ||
        (s.flags & (ARCHBFD_SEC_TLS | ARCHBFD_SEC_RELOC))) goto done;
    if (s.native_type == SHT_INIT_ARRAY || s.native_type == SHT_FINI_ARRAY ||
        s.native_type == SHT_PREINIT_ARRAY || s.native_type == SHT_RELR)
      goto done;
    if (s.native_type == SHT_DYNSYM &&
        (++dynsym != 1 || s.file_offset != elf->dynsym_offset ||
         s.file_size != sizeof elf->symbols)) goto done;
    if (s.native_type == SHT_HASH &&
        (++hash != 1 || s.file_offset != elf->hash_offset ||
         s.file_size != elf->hash_size)) goto done;
    if (s.native_type == SHT_DYNAMIC &&
        (++dynamic != 1 || s.file_offset != elf->dynamic_offset ||
         s.file_size != elf->ph[elf->dynamic_index].p_filesz)) goto done;
    if (s.native_type == SHT_STRTAB && s.file_offset == elf->dynstr_offset &&
        s.file_size == elf->dynstr_size) ++dynstr_count;
  }
  if (dynsym != 1 || hash != 1 || dynamic != 1 || dynstr_count != 1) goto done;
  for (size_t i = 0; i < info.symbol_count; ++i) {
    archbfd_symbol s;
    if (archbfd_get_symbol(obj, ARCHBFD_STATIC_SYMBOLS, i, &s, NULL) != ARCHBFD_OK ||
        s.kind == ARCHBFD_SYMBOL_INDIRECT || s.kind == ARCHBFD_SYMBOL_TLS ||
        (i && (s.flags & ARCHBFD_SYM_UNDEFINED))) goto done;
  }
  for (size_t i = 0; i < EXPORT_COUNT; ++i) {
    const size_t slot = elf->slot_for_export[i];
    const Elf64_Sym *sym = &elf->symbols[slot];
    const size_t name_len = strlen(exports[i].symbol);
    archbfd_symbol s;
    archbfd_section sec;
    size_t offset;
    if (archbfd_get_symbol(obj, ARCHBFD_DYNAMIC_SYMBOLS, slot, &s, NULL) != ARCHBFD_OK ||
        s.section != sym->st_shndx || s.value != sym->st_value || s.size != sym->st_size ||
        s.binding != ARCHBFD_BIND_GLOBAL || s.kind != ARCHBFD_SYMBOL_FUNCTION ||
        !(s.flags & ARCHBFD_SYM_EXPORTED) ||
        (s.flags & (ARCHBFD_SYM_UNDEFINED | ARCHBFD_SYM_ABSOLUTE |
                    ARCHBFD_SYM_HIDDEN | ARCHBFD_SYM_IMPORTED)) ||
        s.name.size != name_len || memcmp(s.name.data, exports[i].symbol, name_len) ||
        archbfd_get_section(obj, s.section, &sec, NULL) != ARCHBFD_OK ||
        sec.native_type != SHT_PROGBITS ||
        (sec.native_flags & (SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE)) !=
            (SHF_ALLOC | SHF_EXECINSTR) ||
        s.value < sec.vma || !range(s.value - sec.vma, s.size, sec.file_size) ||
        !file_offset(elf, s.value, s.size, PF_R | PF_X, &offset) ||
        sec.file_offset + (s.value - sec.vma) != offset)
      goto done;
  }
  ok = 1;

done:
  archbfd_close(obj);
  return ok;
}

static int bind_loaded_adapters(struct on1x_kernel_native *owner, const char *path,
                                const struct elf_image *elf) {
  struct link_map *map = NULL;
  Elf64_Addr dynamic = elf->ph[elf->dynamic_index].p_vaddr;
  if (dlinfo(owner->handle, RTLD_DI_LINKMAP, &map) || !map || !map->l_name ||
      strcmp(map->l_name, path) || map->l_addr > UINTPTR_MAX - VADDR_LIMIT ||
      (uintptr_t)map->l_ld != map->l_addr + dynamic) return 0;
  for (size_t i = 0; i < EXPORT_COUNT; ++i) {
    const Elf64_Sym *s = &elf->symbols[elf->slot_for_export[i]];
    void *addr = (void *)(uintptr_t)(map->l_addr + s->st_value);
    void *root = NULL, *symbol = NULL;
    Dl_info info;
    /* Address and identity come from a pre-verified ordinary STT_FUNC.
     * No name lookup that might invoke an IFUNC resolver. */
    if (!dladdr1(addr, &info, &root, RTLD_DL_LINKMAP) || root != map ||
        !dladdr1(addr, &info, &symbol, RTLD_DL_SYMENT) || !symbol ||
        info.dli_saddr != addr || !info.dli_sname ||
        strcmp(info.dli_sname, exports[i].symbol) ||
        memcmp(symbol, s, sizeof *s))
      return 0;
    struct on1x_kernel_native_adapter *a = &owner->adapters[i];
    _Static_assert(sizeof(a->call2) == sizeof(addr) &&
                   sizeof(a->call3) == sizeof(addr), "SysV AMD64 pointer size");
    if (exports[i].arity == 2) memcpy(&a->call2, &addr, sizeof addr);
    else memcpy(&a->call3, &addr, sizeof addr);
  }
  return 1;
}

int on1x_kernel_native_open(
    const void *image, size_t image_size, const void *policy, size_t policy_size,
    const struct on1x_kernel_native_binding *bindings, size_t binding_count,
    struct on1x_kernel_native **out, const char **reason) {
  const char *why = "invalid native image size or output";
  const unsigned char *sealed = MAP_FAILED;
  struct on1x_kernel_native *owner = NULL;
  struct elf_image elf = {0};
  char digest[65], path[ON1X_SEALED_NATIVE_PATH_CAP];
  if (out) *out = NULL;
  if (!out || !image || !image_size || image_size > ON1X_KERNEL_NATIVE_IMAGE_LIMIT)
    goto fail;
  why = "native image is not approved by host policy";
  if (!policy || !policy_size || policy_size > ON1X_KERNEL_NATIVE_POLICY_LIMIT)
    goto fail;
  why = "native kernel provider is unavailable on this host";
  if (sysconf(_SC_PAGESIZE) != PAGE_SIZE) goto fail;
  why = "cannot allocate native image owner";
  owner = calloc(1, sizeof *owner);
  if (!owner) goto fail;
  why = "cannot create immutable native image";
  owner->fd = on1x_sealed_native_create_("on1x-kernel-native");
  if (owner->fd < 0) goto fail;
  for (size_t pos = 0; pos < image_size;) {
    ssize_t n = write(owner->fd, (const unsigned char *)image + pos, image_size - pos);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) goto fail;
    pos += (size_t)n;
  }
  if (on1x_sealed_native_seal_(owner->fd) < 0) goto fail;
  sealed = mmap(NULL, image_size, PROT_READ, MAP_SHARED, owner->fd, 0);
  if (sealed == MAP_FAILED) goto fail;
  why = "native image is not approved by host policy";
  if (!host_approves(sealed, image_size, policy, policy_size,
                     bindings, binding_count, digest)) goto fail;
  why = "native ELF violates the no-import/no-init adapter profile";
  if (!program_headers(sealed, image_size, &elf) || !dynamic_tables(sealed, &elf))
    goto fail;
  why = "qBFD rejected or disagrees with native ELF metadata";
  if (!qbfd_agrees(sealed, image_size, &elf)) goto fail;
  (void)munmap((void *)sealed, image_size);
  sealed = MAP_FAILED;
  why = "procfs access to the sealed native image is unavailable";
  if (!on1x_sealed_native_proc_path_(owner->fd, digest, path)) goto fail;
  why = "OS loader could not open the approved native image";
  owner->handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!owner->handle) goto fail;
  why = "native adapter is not defined by the sealed image";
  if (!bind_loaded_adapters(owner, path, &elf)) goto fail;
  *out = owner;
  if (reason) *reason = NULL;
  return 0;

fail:
  if (sealed != MAP_FAILED) (void)munmap((void *)sealed, image_size);
  on1x_kernel_native_close(owner);
  if (reason) *reason = why;
  return -1;
}

int on1x_kernel_native_get(const struct on1x_kernel_native *owner,
    const char *export_name, unsigned arity, struct on1x_kernel_native_adapter *out) {
  size_t i = export_index(export_name, arity);
  if (!owner || !out || i == EXPORT_COUNT) return -1;
  *out = owner->adapters[i];
  return 0;
}

void on1x_kernel_native_close(struct on1x_kernel_native *owner) {
  if (!owner) return;
  if (owner->handle) (void)dlclose(owner->handle);
  if (owner->fd >= 0) (void)close(owner->fd);
  free(owner);
}

#else

int on1x_kernel_native_open(
    const void *image, size_t image_size, const void *policy, size_t policy_size,
    const struct on1x_kernel_native_binding *bindings, size_t binding_count,
    struct on1x_kernel_native **out, const char **reason) {
  (void)image; (void)image_size; (void)policy; (void)policy_size;
  (void)bindings; (void)binding_count;
  if (out) *out = NULL;
  if (reason) *reason = "native kernel provider is unavailable on this host";
  return -1;
}

int on1x_kernel_native_get(const struct on1x_kernel_native *owner,
    const char *export_name, unsigned arity, struct on1x_kernel_native_adapter *out) {
  (void)owner; (void)export_name; (void)arity; (void)out;
  return -1;
}

void on1x_kernel_native_close(struct on1x_kernel_native *owner) { (void)owner; }

#endif
