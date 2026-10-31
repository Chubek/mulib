# mulib-conainerd-ipc

## NAME AND INTEGRATION

Container daemon command and status records. The requested `conainerd` spelling
is retained in header, switch and public symbols. Define `MULIB_IMPL_CONAINERD_IPC`
plus `container-ipc`, `crc32`, `container`. Example:
`examples/modules/conainerd-ipc.c`.

## COMMANDS

Opcodes 1–7 are START, STOP, DELETE, STATUS, PAUSE, RESUME, STEP. Every request
payload is exactly 96 bytes:

* Bytes 0–63: validated ID, NUL-terminated and zero-padded.
* Bytes 64–71: u64 expected generation, zero disables compare-and-swap.
* Bytes 72–79: u64 caller time.
* Bytes 80–87: u64 value.
* Bytes 88–95: zero reserved bytes.

STOP requires value 1–64 (signal number). STEP requires a positive work budget
representable by local size_t. Other commands require zero value. `command`
encodes this payload in an IPC request; `parse` accepts a decoded request and
validates canonical padding, opcode, flags and value constraints.

Transport authentication is separate from the command. The control bridge uses
its trusted server time instead of the wire time. Direct daemon callers are
responsible for supplying trusted monotonic time.

## RESPONSES

`reply(command,result,status,out)` correlates opcode/request ID and sets the
response flag. Negative CT results have an empty payload. Success has exactly
48 bytes: u32 state, i32 exit code, followed by u64 generation, starts, started_at,
exited_at and polls. No pointers or native padding are serialized.

`status(message,command,out)` verifies correlation and decodes a success snapshot.
Remote negative errors are returned directly with unchanged output. Unknown
state, bad length, mismatched opcode/ID and incorrect flags fail with EFORMAT.

## OWNERSHIP AND ERRORS

Commands/statuses are freely copyable fixed records. Encode outputs are initialized
CT blobs; decoded commands own their fixed ID bytes. All result values are CT
codes. Creating/importing images is a direct daemon API; these lifecycle commands
do not embed arbitrary image uploads or transport credentials.
