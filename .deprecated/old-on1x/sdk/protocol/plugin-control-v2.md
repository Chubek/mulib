# Plugin control protocol 2

All integers are unsigned big-endian. A MuLIB framed-IPC length prefix encloses
one protocol frame. The maximum control frame is
`ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES`.

## Fixed header

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic `O1XP` (`0x4f315850`) |
| 4 | 2 | protocol major (`2`) |
| 6 | 2 | protocol minor |
| 8 | 2 | message kind |
| 10 | 2 | flags |
| 12 | 4 | `On1xSDK_Status` |
| 16 | 8 | request ID |
| 24 | 8 | owner context |
| 32 | 8 | object ID |
| 40 | 8 | revision |
| 48 | 4 | payload byte count |
| 52 | 4 | reserved, zero |
| 56 | N | payload |

The decoder rejects unknown major versions, unknown kinds or flags, oversized
payloads, nonzero reserved bytes, length mismatches, and invalid status/kind
combinations.

## Lifecycle

The one-shot isolated worker uses:

```text
host -> HELLO(setup)
worker -- establish all isolation -->
worker -> HELLO_ACK
host -> ACTIVATE
worker -> ACTIVE
host -> REQUEST(request=1)
worker -> RESPONSE(request=1, FINAL)
```

The setup payload contains only an internal setup version, operation mode,
bounded hook name, a frozen input descriptor, and a staging-output descriptor.
The worker attaches both mappings before entering a new IPC namespace. Plugin
code is not loaded at this stage.

A successful response sets `SHARED_PAYLOAD|FINAL` and carries one
`On1xSDK_SharedMemoryDescriptor`. A failed response carries no payload.
Cancellation and shutdown are state-checked protocol operations for reusable
sessions; the compatibility worker is one-shot and is killed by its supervisor
on timeout or protocol failure.

## Shared-memory descriptor

The descriptor schema is independently versioned and 88 bytes. It contains no
pointer:

```text
schema major/minor, flags, owner context, generation handle, kind, state,
platform ID, reserved, capacity, used size, SHA-256
```

Staging descriptors have state `CREATED`, size zero, and a zero digest. Frozen
descriptors have state `FROZEN` and a digest over exactly `used size` bytes.
The receiver validates bounds and authorization before attachment.
