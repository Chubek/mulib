# mulib-midi

[Manuals](README.md) · [Library index](index.md)

Owned event sequences and Standard MIDI File (SMF) IO, independent of platform
MIDI devices. Define `MULIB_IMPL_MIDI`, `MULIB_IMPL_AUDIO`, `MULIB_IMPL_XSIMD`;
include `mulib-midi.h`. Example: `examples/modules/midi.c`.

Zero-initialize a sequence, then `init(&sequence,ppqn,tracks)`. PPQN is 1..32767;
track count is 1..65535. `event` copies payload bytes and records absolute
32-bit ticks and a zero-based track. Channel statuses contain a zero-based
channel (0..15); data bytes are 7-bit. Program/channel pressure have one byte;
other channel messages have two. FF meta events retain type and arbitrary
payload; F0/F7 SysEx retain their payload. Other system statuses are unsupported.
`note` atomically appends note-on and note-off with a positive tick duration.
`tempo` writes microseconds per quarter note (not BPM).

`encode` writes type 0 for one track, type 1 otherwise. Events are sorted by
track, tick, then insertion order, without changing the input. Equal-time order
is caller-controlled: insert note-offs before retriggering notes when desired.
No running status is emitted. Deltas and payload lengths must fit four-byte
VLQs (28 bits). End-of-track is generated automatically; an explicit FF/2F
zero-length event can retain trailing silence, but must follow every other
event in its track. Output bytes must be empty. Failure preserves them.

`decode` accepts SMF0/1, running status, meta/SysEx, extended MThd length,
and requires complete tracks ending in EOT. Payload lengths, VLQ width,
tick overflow, channel bytes and track boundaries are checked. SMPTE timing
and SMF2 are unsupported. The event limit defaults to one million; set it lower
for small applications. The decoder copies all data; failure preserves an empty
output sequence. Free with `mulib_midi_free`. Do not mutate ownership fields
or event storage directly. `encode` may allocate an event-pointer sort index
in addition to the output file.

Audrey adds Lua score notation and compiles beat-based notes into this model.
MIDI files describe performance events; they do not contain synthesized audio.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-midi.h`](../include/mulib-midi.h) |
| Implementation switch | `MULIB_IMPL_MIDI` |
| Repository-local includes | [mulib-audio](mulib-audio.md) |
| Complete example | [`examples/modules/midi.c`](../examples/modules/midi.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/midi.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
