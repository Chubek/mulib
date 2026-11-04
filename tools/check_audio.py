#!/usr/bin/env python3
"""Optional independent audio-format oracles; never used by the library."""
import argparse
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--codec', required=True)
    parser.add_argument('--ffmpeg', required=True)
    parser.add_argument('--flac', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='mulib-audio-', dir='/tmp/opencode' if Path('/tmp/opencode').is_dir() else None) as work:
        def run(*command):
            subprocess.run(command, cwd=work, check=True, capture_output=True)

        def floats(name):
            data = (Path(work) / name).read_bytes()
            return struct.unpack('<%df' % (len(data) // 4), data)

        run(args.codec, '--fixtures')
        for codec, extension in [('vorbis', 'ogg'), ('mpeg', 'mp3')]:
            run(args.ffmpeg, '-v', 'error', '-y', '-i', 'encoded.' + extension,
                '-f', 'f32le', 'reference.f32')
            run(args.ffmpeg, '-v', 'error', '-y', '-i', 'own-' + codec + '.wav',
                '-f', 'f32le', 'own.f32')
            reference, own = floats('reference.f32'), floats('own.f32')
            assert len(reference) == len(own), (codec, 'frame count')
            assert all(math.isfinite(v) for v in own)
            error = max(abs(x-y) for x, y in zip(reference, own))
            assert error < 1e-5, (codec, error)
            print(codec, 'independent PCM comparison:', error)
        run(args.flac, '-t', 'encoded16.flac', 'encoded24.flac')
        # Exercise reference predictor/residual and stereo-decorrelation coding.
        for level in ('0', '8'):
            run(args.flac, '-f', '-' + level, 'source.wav', '-o', 'reference.flac')
            run(args.codec, '--decode-flac', 'reference.flac')
            run(args.ffmpeg, '-v', 'error', '-y', '-i', 'source.wav', '-f', 'f32le', 'source.f32')
            run(args.ffmpeg, '-v', 'error', '-y', '-i', 'decoded-flac.wav', '-f', 'f32le', 'own.f32')
            assert floats('source.f32') == floats('own.f32'), ('FLAC predictor level', level)
        print('FLAC independent validation and exact predictor decoding: ok')


if __name__ == '__main__':
    main()
