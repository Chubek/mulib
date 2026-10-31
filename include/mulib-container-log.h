/** @file mulib-container-log.h
 * @brief Bounded binary log ring with sequence cursors, filtering and loss accounting.
 * Define MULIB_IMPL_CONTAINER_LOG once. */
#ifndef MULIB_CONTAINER_LOG_H
#define MULIB_CONTAINER_LOG_H
#include "include-mulib-container-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_container_log_level
{
  MULIB_CONTAINER_LOG_TRACE, MULIB_CONTAINER_LOG_DEBUG, MULIB_CONTAINER_LOG_INFO,
  MULIB_CONTAINER_LOG_WARN, MULIB_CONTAINER_LOG_ERROR
} mulib_container_log_level;
typedef struct mulib_container_log_record
{
  uint64_t sequence, timestamp;
  mulib_container_log_level level;
  unsigned stream;
  char container_id[MULIB_CT_ID_CAP];
  mulib_ct_blob message;
} mulib_container_log_record;
/** @brief Owns records; record pointers survive reads but not mutation/free. */
typedef struct mulib_container_log
{
  mulib_container_log_record *records;
  size_t capacity, count, head, bytes, max_bytes, max_message;
  uint64_t next_sequence, dropped, filtered;
  mulib_container_log_level minimum;
  int overwrite;
} mulib_container_log;
/** @brief Allocate a bounded ring. overwrite=0 rejects a full ring atomically. */
int mulib_container_log_init (mulib_container_log *log, size_t records,
                              size_t max_bytes, size_t max_message, int overwrite);
/** @brief Release all owned messages. */
void mulib_container_log_free (mulib_container_log *log);
/** @brief Append copied binary data. Below-minimum records increment filtered
 * and return OK with sequence zero. Stream is application-defined (1 stdout, 2 stderr). */
int mulib_container_log_append (mulib_container_log *log, const char *container_id,
    mulib_container_log_level level, unsigned stream, uint64_t timestamp,
    const void *message, size_t length, uint64_t *sequence);
/** @brief Read the first matching record at/after *cursor; zero starts oldest.
 * ESTALE advances a lost cursor to the oldest retained sequence. AGAIN means
 * no match yet. Optional id filter; success advances past the returned record. */
int mulib_container_log_next (const mulib_container_log *log, uint64_t *cursor,
    const char *container_id, mulib_container_log_level minimum,
    const mulib_container_log_record **record);
/** @brief Drop all records with sequence less than before; explicit pruning
 * does not increment the overwrite-loss metric. */
size_t mulib_container_log_prune (mulib_container_log *log, uint64_t before);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_LOG) && !defined(MULIB_CONTAINER_LOG_IMPLEMENTED)
#define MULIB_CONTAINER_LOG_IMPLEMENTED
int
mulib_container_log_init (mulib_container_log *l, size_t count, size_t bytes,
                          size_t message, int overwrite)
{
  mulib_container_log next;
  if (!l || !count || count > SIZE_MAX / sizeof (*next.records) || message > bytes)
    return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  next.records = (mulib_container_log_record *)MULIB_CT_MALLOC (count * sizeof (*next.records));
  if (!next.records) return MULIB_CT_ENOMEM;
  memset (next.records, 0, count * sizeof (*next.records));
  next.capacity = count; next.max_bytes = bytes; next.max_message = message;
  next.overwrite = overwrite != 0; next.next_sequence = 1; *l = next;
  return 0;
}
static void mulib_container_log_pop_ (mulib_container_log *l)
{
  mulib_container_log_record *r = l->records + l->head;
  l->bytes -= r->message.length; mulib_ct_blob_free (&r->message);
  memset (r, 0, sizeof (*r)); l->head = (l->head + 1) % l->capacity; --l->count;
}
void mulib_container_log_free (mulib_container_log *l)
{
  if (!l) return;
  while (l->count) mulib_container_log_pop_ (l);
  MULIB_CT_FREE (l->records); memset (l, 0, sizeof (*l));
}
int
mulib_container_log_append (mulib_container_log *l, const char *id,
    mulib_container_log_level level, unsigned stream, uint64_t time,
    const void *data, size_t n, uint64_t *sequence)
{
  mulib_container_log_record r;
  int rc;
  if (sequence) *sequence = 0;
  if (!l || !l->capacity || !mulib_ct_id_valid (id) || (!data && n)
      || level < 0 || level > MULIB_CONTAINER_LOG_ERROR) return MULIB_CT_EINVAL;
  if (level < l->minimum) { l->filtered = mulib_ct_counter_add (l->filtered, 1); return 0; }
  if (n > l->max_message || n > l->max_bytes || l->next_sequence == UINT64_MAX)
    return MULIB_CT_ELIMIT;
  if (!l->overwrite && (l->count == l->capacity || n > l->max_bytes - l->bytes))
    return MULIB_CT_ELIMIT;
  memset (&r, 0, sizeof (r));
  rc = mulib_ct_blob_assign (&r.message, data, n, l->max_message);
  if (rc) return rc;
  (void)mulib_ct_text_copy (r.container_id, sizeof (r.container_id), id);
  r.sequence = l->next_sequence++; r.timestamp = time; r.level = level; r.stream = stream;
  while (l->count == l->capacity || n > l->max_bytes - l->bytes)
    { mulib_container_log_pop_ (l); l->dropped = mulib_ct_counter_add (l->dropped, 1); }
  l->records[(l->head + l->count) % l->capacity] = r; ++l->count; l->bytes += n;
  if (sequence) *sequence = r.sequence;
  return 0;
}
int
mulib_container_log_next (const mulib_container_log *l, uint64_t *cursor,
    const char *id, mulib_container_log_level minimum, const mulib_container_log_record **out)
{
  size_t i;
  uint64_t oldest;
  if (!l || !l->capacity || !cursor || !out || (id && !mulib_ct_id_valid (id))
      || minimum < 0 || minimum > MULIB_CONTAINER_LOG_ERROR) return MULIB_CT_EINVAL;
  oldest = l->count ? l->records[l->head].sequence : l->next_sequence;
  if (!*cursor) *cursor = oldest;
  if (*cursor < oldest) { *cursor = oldest; return MULIB_CT_ESTALE; }
  for (i = 0; i < l->count; ++i)
    {
      const mulib_container_log_record *r = l->records + (l->head + i) % l->capacity;
      if (r->sequence < *cursor) continue;
      *cursor = r->sequence + 1;
      if (r->level >= minimum && (!id || !strcmp (r->container_id, id)))
        { *out = r; return 0; }
    }
  return MULIB_CT_AGAIN;
}
size_t mulib_container_log_prune (mulib_container_log *l, uint64_t before)
{
  size_t count = 0;
  if (!l) return 0;
  while (l->count && l->records[l->head].sequence < before)
    { mulib_container_log_pop_ (l); ++count; }
  return count;
}
#endif
