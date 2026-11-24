/** @file mulib-cache.h
 * @brief Explicit disk snapshots with selectable sorted KVDB, HAT-trie or trie indexes.
 * Define MULIB_IMPL_CACHE and dependencies KVDB, BUFFER, VARINT, CRC32, ENDIAN,
 * HATTRIE, HASH and TRIE. No implicit writes, eviction, expiry or global cache.
 */
#ifndef MULIB_CACHE_H
#define MULIB_CACHE_H
#include "mulib-kvdb.h"
#include "mulib-hattrie.h"
#include "mulib-trie.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_cache_backend {
  MULIB_CACHE_SORTED, MULIB_CACHE_HATTRIE, MULIB_CACHE_TRIE
} mulib_cache_backend_t;
typedef struct mulib_cache mulib_cache_t;
/** Create a memory-only cache, or open/create a bound disk snapshot. */
mulib_cache_t *mulib_cache_create(mulib_cache_backend_t);
int mulib_cache_open(const char *, int create, mulib_cache_backend_t, mulib_cache_t **);
/** Close discards uncommitted changes. All return codes are KVDB result codes. */
void mulib_cache_destroy(mulib_cache_t *);
int mulib_cache_set(mulib_cache_t *, const void *, size_t, const void *, size_t);
int mulib_cache_get(const mulib_cache_t *, const void *, size_t, const void **, size_t *);
int mulib_cache_remove(mulib_cache_t *, const void *, size_t);
size_t mulib_cache_size(const mulib_cache_t *);
/** Borrow an ordered entry. Views are invalidated by mutation/destruction. */
int mulib_cache_at(const mulib_cache_t *, size_t, const void **, size_t *, const void **, size_t *);
int mulib_cache_commit(mulib_cache_t *);
int mulib_cache_save(const mulib_cache_t *, const char *);
int mulib_cache_serialize(const mulib_cache_t *, mulib_buffer_t *);
/** Format is independent of index choice. out unchanged on failure. */
int mulib_cache_deserialize(const void *, size_t, mulib_cache_backend_t, mulib_cache_t **);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CACHE) && !defined(MULIB_CACHE_IMPLEMENTED)
#define MULIB_CACHE_IMPLEMENTED
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_cache_entry_ { const void *value; size_t length; int active; } mulib_cache_entry_;
struct mulib_cache {
  mulib_cache_backend_t backend; mulib_kvdb_t *db; mulib_hattrie_t hat; mulib_trie_t trie;
  mulib_cache_entry_ *entries; size_t slots, capacity;
};
static int mulib_cache_index_(mulib_cache_t *c, const void *k, size_t n, uint64_t id) {
  return c->backend == MULIB_CACHE_HATTRIE ? mulib_hattrie_set(&c->hat, k, n, id) :
    c->backend == MULIB_CACHE_TRIE ? mulib_trie_set(&c->trie, k, n, id) : 1;
}
static int mulib_cache_find_(const mulib_cache_t *c, const void *k, size_t n, uint64_t *id) {
  return c->backend == MULIB_CACHE_HATTRIE ? mulib_hattrie_get(&c->hat, k, n, id) : mulib_trie_get(&c->trie, k, n, id);
}
static void mulib_cache_unindex_(mulib_cache_t *c, const void *k, size_t n) {
  if (c->backend == MULIB_CACHE_HATTRIE) mulib_hattrie_remove(&c->hat, k, n, NULL);
  if (c->backend == MULIB_CACHE_TRIE) mulib_trie_remove(&c->trie, k, n);
}
void mulib_cache_destroy(mulib_cache_t *c) {
  if (c) { mulib_kvdb_close(c->db); mulib_hattrie_free(&c->hat); mulib_trie_free(&c->trie); free(c->entries); free(c); }
}
static int mulib_cache_adopt_(mulib_kvdb_t *db, mulib_cache_backend_t backend, mulib_cache_t **out) {
  mulib_cache_t *c;
  size_t i;
  if (backend < MULIB_CACHE_SORTED || backend > MULIB_CACHE_TRIE || !out) {
    mulib_kvdb_close(db); return MULIB_KVDB_EINVAL;
  }
  c = (mulib_cache_t *)calloc(1, sizeof *c);
  if (!c) { mulib_kvdb_close(db); return MULIB_KVDB_ENOMEM; }
  c->db = db; c->backend = backend;
  if (backend != MULIB_CACHE_SORTED && mulib_kvdb_size(db)) {
    c->entries = (mulib_cache_entry_ *)calloc(mulib_kvdb_size(db), sizeof *c->entries);
    if (!c->entries) { mulib_cache_destroy(c); return MULIB_KVDB_ENOMEM; }
    c->slots = c->capacity = mulib_kvdb_size(db);
  }
  for (i = 0; i < mulib_kvdb_size(db); ++i) {
    const void *k, *v; size_t n, vn;
    mulib_kvdb_at(db, i, &k, &n, &v, &vn);
    if (!mulib_cache_index_(c, k, n, (uint64_t)i)) { mulib_cache_destroy(c); return MULIB_KVDB_ENOMEM; }
    if (c->entries) { c->entries[i].value = v; c->entries[i].length = vn; c->entries[i].active = 1; }
  }
  *out = c; return MULIB_KVDB_OK;
}
mulib_cache_t *mulib_cache_create(mulib_cache_backend_t b) {
  mulib_cache_t *c = NULL;
  mulib_kvdb_t *db = mulib_kvdb_create();
  if (db) mulib_cache_adopt_(db, b, &c);
  return c;
}
int mulib_cache_open(const char *p, int create, mulib_cache_backend_t b, mulib_cache_t **out) {
  mulib_kvdb_t *db;
  int rc;
  if (!out || b < MULIB_CACHE_SORTED || b > MULIB_CACHE_TRIE) return MULIB_KVDB_EINVAL;
  rc = mulib_kvdb_open(p, create, &db);
  return rc ? rc : mulib_cache_adopt_(db, b, out);
}
int mulib_cache_set(mulib_cache_t *c, const void *k, size_t n, const void *v, size_t vn) {
  const void *stored; size_t stored_n, slot;
  unsigned char *key;
  uint64_t id = 0;
  int present, rc;
  if (!c || (!k && n) || (!v && vn)) return MULIB_KVDB_EINVAL;
  if (c->backend == MULIB_CACHE_SORTED) return mulib_kvdb_set(c->db, k, n, v, vn);
  present = mulib_cache_find_(c, k, n, &id);
  slot = present ? (size_t)id : 0;
  if (!present) {
    while (slot < c->slots && c->entries[slot].active) ++slot;
    if (slot == c->capacity) {
      size_t cap = c->capacity ? c->capacity * 2 : 16;
      mulib_cache_entry_ *entries;
      if (cap <= c->capacity || cap > SIZE_MAX / sizeof *entries) return MULIB_KVDB_EOVERFLOW;
      entries = (mulib_cache_entry_ *)realloc(c->entries, cap * sizeof *entries);
      if (!entries) return MULIB_KVDB_ENOMEM;
      memset(entries + c->capacity, 0, (cap - c->capacity) * sizeof *entries);
      c->entries = entries; c->capacity = cap;
    }
  }
  key = (unsigned char *)malloc(n ? n : 1); if (!key) return MULIB_KVDB_ENOMEM;
  if (n) memcpy(key, k, n);
  if (!mulib_cache_index_(c, key, n, (uint64_t)slot)) { free(key); return MULIB_KVDB_ENOMEM; }
  rc = mulib_kvdb_set(c->db, key, n, v, vn);
  if (rc && !present) mulib_cache_unindex_(c, key, n);
  if (!rc) {
    mulib_kvdb_get(c->db, key, n, &stored, &stored_n);
    c->entries[slot].value = stored; c->entries[slot].length = stored_n; c->entries[slot].active = 1;
    if (slot == c->slots) ++c->slots;
  }
  free(key); return rc;
}
int mulib_cache_get(const mulib_cache_t *c, const void *k, size_t n, const void **v, size_t *vn) {
  uint64_t id;
  if (!c || (!k && n) || !v || !vn) return MULIB_KVDB_EINVAL;
  if (c->backend == MULIB_CACHE_SORTED) return mulib_kvdb_get(c->db, k, n, v, vn);
  if (!mulib_cache_find_(c, k, n, &id)) return MULIB_KVDB_ENOTFOUND;
  *v = c->entries[(size_t)id].value; *vn = c->entries[(size_t)id].length; return MULIB_KVDB_OK;
}
int mulib_cache_remove(mulib_cache_t *c, const void *k, size_t n) {
  int rc, present; uint64_t id = 0;
  if (!c) return MULIB_KVDB_EINVAL;
  /* Unindex first: k may be a borrowed database key. Removal never allocates. */
  if ((!k && n)) return MULIB_KVDB_EINVAL;
  present = c->backend != MULIB_CACHE_SORTED && mulib_cache_find_(c, k, n, &id);
  mulib_cache_unindex_(c, k, n); rc = mulib_kvdb_remove(c->db, k, n);
  if (!rc && present) memset(&c->entries[(size_t)id], 0, sizeof *c->entries);
  return rc;
}
size_t mulib_cache_size(const mulib_cache_t *c) { return c ? mulib_kvdb_size(c->db) : 0; }
int mulib_cache_at(const mulib_cache_t *c, size_t i, const void **k, size_t *n, const void **v, size_t *vn) {
  return c ? mulib_kvdb_at(c->db, i, k, n, v, vn) : MULIB_KVDB_EINVAL;
}
int mulib_cache_commit(mulib_cache_t *c) { return c ? mulib_kvdb_commit(c->db) : MULIB_KVDB_EINVAL; }
int mulib_cache_save(const mulib_cache_t *c, const char *p) { return c ? mulib_kvdb_save(c->db, p) : MULIB_KVDB_EINVAL; }
int mulib_cache_serialize(const mulib_cache_t *c, mulib_buffer_t *out) { return c ? mulib_kvdb_serialize(c->db, out) : MULIB_KVDB_EINVAL; }
int mulib_cache_deserialize(const void *p, size_t n, mulib_cache_backend_t b, mulib_cache_t **out) {
  mulib_kvdb_t *db; int rc;
  if (!out || b < MULIB_CACHE_SORTED || b > MULIB_CACHE_TRIE) return MULIB_KVDB_EINVAL;
  rc = mulib_kvdb_deserialize(p, n, &db); return rc ? rc : mulib_cache_adopt_(db, b, out);
}
#ifdef __cplusplus
}
#endif
#endif
