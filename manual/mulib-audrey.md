# mulib-audrey

[Manuals](README.md) · [Library index](index.md)

Offline, dependency-free audio editing and a small DAW-style render/score layer.
It uses MuLIB codecs, xsimd and Mune; no third-party codec or Lua libraries.
It is not a realtime audio-device engine, plugin host, GUI DAW or instrument bank.

## Build

Define `MULIB_IMPL_AUDREY`, `MULIB_IMPL_AUDIO`, `MULIB_IMPL_XSIMD`,
`MULIB_IMPL_MINIWAV`, `MULIB_IMPL_MINIFLAC`, `MULIB_IMPL_MINIVORBIS`,
`MULIB_IMPL_MINIMPEG`, `MULIB_IMPL_MIDI`, plus Mune's transitive implementation
switches: `MUNE`, `DYNLOAD`, `MIR`, `VMWEAVE`, `EGRAPH`, `REWRITE`, `DYNCALL`,
`JIT`, `PARSEC`, `PARSER`, `PEG`, `AST`, `STRBUF`, `GLOB` (all prefixed
`MULIB_IMPL_`). Include `mulib-audrey.h`. Linux links `-lm -pthread -ldl`.
The complete runnable setup is in `examples/modules/audrey.c`.

## Files and editing

`load` detects WAV, native FLAC, Ogg/Vorbis or MP3 by magic. `save` selects
`"wav"`, `"flac"`, `"vorbis"` or `"mp3"` explicitly. PCM bit depth applies to
WAV/FLAC; it is ignored for lossy formats. The codecs' documented profile
restrictions apply, particularly the limited MP3 and Vorbis decoders. Load
enforces the file helper's 256 MiB input limit and the caller's decoded-frame
limit. Save errors can leave a partial file.

All positions/durations in editing are **frames**, not interleaved sample
indices or seconds. `slice` copies `[start,start+length)` into an empty owned
output. `cut` removes it and closes the gap. `insert` inserts same-format PCM,
including self-insertion. Bounds/overflow failures preserve the input.
`resample` provides linear interpolation with ceiling-rounded output length;
it is not a band-limited antialiasing resampler. Convert rates explicitly before
placing clips on a timeline. Channels retain their original ordering.

## Tracks and native effects

`render` accepts borrowed clips and track settings, allocating a fixed-length
output. Clips select a source range and destination start, track, gain and
stereo balance pan [-1,1]. Set gains explicitly (normally 1). Clips must fit the
timeline and match its rate/channels. Overlaps are summed, track gain/mute/solo
apply, and no implicit clipping or limiting is performed. Center pan preserves
both channels; endpoints silence one side. Mono pan must be zero. Muted tracks
remain silent even when soloed. Invalid/nonfinite render output is discarded.

`normalize` adjusts peak amplitude (silence stays silent), `fade` multiplies by
a linear frame envelope, and `lowpass` applies a one-pole lowpass with independent
channel history initialized to zero. `effect` runs a user C callback on a copy
of PCM, committing only success with finite output. The callback receives a
borrowed interleaved buffer, frame count, channels, rate and user context.
Callbacks must not free/resize the buffer. External callback side effects are
not rolled back.

## Lua filters

Create a Mune state and compile source returning a function:

```lua
local gain = 0.5
return function(sample, channel, frame, rate)
  return sample * gain
end
```

`filter_compile` roots the returned function; `filter_apply` calls it once per
sample in interleaved order. Channel is one-based, frame zero-based, and rate Hz.
Return one finite numeric sample. Closures can retain histories for delays,
oscillators or stateful filters. For example, a one-pole effect:

```lua
local history = {}
return function(x, channel, frame, rate)
  local a = 1 - math.exp(-2 * math.pi * 1000 / rate)
  local y = (history[channel] or 0) + a * (x - (history[channel] or 0))
  history[channel] = y
  return y
end
```

PCM is transactional; Lua errors or nonfinite return values discard the output
copy. Lua state changes before errors persist. Mune limits apply per invocation,
not across the whole render. Per-sample Lua is for offline processing, not
hard-realtime use. State standard libraries are available as configured by Mune;
this is not a separate sandbox. `filter_free` releases the root; destroy filters
before their borrowed state. Mune runtime diagnostics borrow state storage.
For a wrong return type/nonfinite value, Audrey returns `SCRIPT`; the Mune error
object may still describe the successful Lua call, so check the audio status.

## Lua music sheets to MIDI

`score` executes Lua returning a dense score table, e.g.:

```lua
local notes = {}
for i, key in ipairs({60, 64, 67, 72}) do
  notes[i] = {beat=i-1, duration=0.9, key=key, velocity=100}
end
return {tempo=120, ppqn=480,
        tracks={{channel=1, program=0, notes=notes}}}
```

Use `mulib_midi_encode` and `mulib_audio_write_file` to output the MIDI file.
Tempo defaults to 120 BPM (range 4..1000); PPQN defaults to 480. MIDI channel is
1..16, program/key 0..127, velocity 1..127 (defaults 1/0/100 respectively).
Beat zero starts the song; durations are positive quarter-note counts. Boundaries
round to nearest tick, and durations that collapse to zero ticks fail. Each
track receives a program change at tick zero; a separate tempo track is added.
Scores are bounded to 100000 notes and 65534 musical tracks. Arrays with holes
or non-index keys are rejected. Score fields use raw access, not metamethods.
Failure preserves an empty output MIDI sequence, but Lua side effects persist.
The output is performance-event MIDI, not synthesized instrumental audio.

See also [audio](mulib-audio.md), [Mune](mulib-mune.md), [MIDI](mulib-midi.md)
and [xsimd](mulib-xsimd.md) for ownership, failure and implementation details.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-audrey.h`](../include/mulib-audrey.h) |
| Implementation switch | `MULIB_IMPL_AUDREY` |
| Repository-local includes | [mulib-midi](mulib-midi.md), [mulib-miniflac](mulib-miniflac.md), [mulib-minimpeg](mulib-minimpeg.md), [mulib-minivorbis](mulib-minivorbis.md), [mulib-miniwav](mulib-miniwav.md), [mulib-mune](mulib-mune.md) |
| Complete example | [`examples/modules/audrey.c`](../examples/modules/audrey.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/audrey.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
