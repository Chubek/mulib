# mulib-containerd-cache

## NAME AND INTEGRATION

Bounded validated-image cache with LRU eviction, expiry and pinned leases.
Define `MULIB_IMPL_CONTAINERD_CACHE` and the `imgformat` closure. Example:
`examples/modules/containerd-cache.c`.

## INSERTION AND EVICTION

`init(cache,entries,bytes)` allocates stable fixed slots and sets an aggregate
image-byte budget. `put(key,image,length,now,ttl)` validates the image envelope,
copies bytes and plans all required evictions before mutating cache. Configuration
grammar/archive semantics are checked when materializing the cached image.

Keys use container ID syntax. Replacing a pinned key returns EBUSY. Unpinned
least-recently-used entries are selected until both slot and byte quotas fit;
if pinned entries prevent this, put returns EBUSY and preserves the entire cache.
Allocation failure also preserves contents. Peak temporary memory includes one
incoming image copy plus an entry-mark array.

TTL zero never expires. Otherwise expiration is now+ttl with checked overflow;
acquisition at exactly that time is a miss. LRU uses an internal access sequence,
independent of caller time. Sequence saturation resets ranks to a deterministic
tie order. Generation never wraps: exhausted generations reject insertion.

## LEASES

`acquire(key,now,&lease,&image)` increments pins and returns a borrowed blob.
Bytes remain valid until that acquisition is released, even after expiry.
Lease stores slot/generation and must be released exactly once. `release`
rejects stale slots/generations and unpinned entries. This is reference accounting,
not a transferable unique capability; do not duplicate a lease and release twice.

`remove(key)` deletes only unpinned entries. `sweep(now)` removes all expired,
unpinned entries and returns their count. `free` preflights all pins and returns
EBUSY unchanged if any remain. Otherwise all owned images/table are released.

## STATISTICS AND RESULTS

hits/misses/evictions saturate; replacement of the same key is not an eviction.
Current count/bytes reflect retained images, including expired pinned ones.
Uses CT errors, including image CRC errors. Cache does not authenticate images,
write persistent storage or start an eviction thread. Externally serialize use.
