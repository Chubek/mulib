# mulib-container-ipc

## NAME AND INTEGRATION

Bounded checksummed frames with incremental one-message stream assembly.
Define `MULIB_IMPL_CONTAINER_IPC` and `MULIB_IMPL_CRC32` once. Example:
`examples/modules/container-ipc.c`.

## WIRE FORMAT

Every frame has a 32-byte header followed by opaque payload. All integers are LE.

| Offset | Field |
| --- | --- |
| 0 | `MCIP` magic |
| 4 | u8 version, 1 |
| 5 | u8 flags: 0 request, 1 response |
| 6 | nonzero u16 opcode |
| 8 | u64 request ID |
| 16 | u32 payload length |
| 20 | signed two's-complement i32 status: request 0, response 0 through -14 |
| 24 | IEEE CRC32 of payload |
| 28 | IEEE CRC32 of header bytes 0–27 |

Unknown flags/status ranges, zero opcodes and noncanonical versions fail. Generic
framing allows request ID zero; higher protocols may require nonzero IDs. CRC is
corruption detection, with authentication provided by the transport/application.

## ENCODING AND DECODING

`encode(message,out,max_payload)` replaces an initialized CT blob atomically;
payload can alias old output. `encode_into` uses nonoverlapping caller storage,
allocates nothing, and zeroes written before validation. Payload limit and 32-bit
wire length are checked independently of size_t overflow.

`decode(data,length,max_payload,&message,&consumed)` returns zero for one complete
frame, AGAIN for incomplete input, or a negative error. Output borrows input and
changes only on success. consumed remains zero on failure. Trailing frames are
not consumed, allowing application-level multiplexing.

## STREAM READER

`reader_init` creates an empty bounded accumulator. `feed` copies fragments and
consumes through at most one frame. It validates the header before allocating
payload space; DONE means peek is available. While ready, feed consumes zero.
`peek` returns a borrowed message valid until reset/free. `reset` discards ready,
partial or failed state while retaining bounded storage. `reader_free` releases it.

Malformed/checksum/limit/allocation errors are sticky until reset. Backpressure
is explicit: drain a ready message before accepting another. Tests exercise every
split point, empty payloads, quota failures and checksum corruption.
