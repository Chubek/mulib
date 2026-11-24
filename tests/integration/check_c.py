"""Build the C-only workload chains and compare optional local providers."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def run(*command, stdin=None):
    proc = subprocess.run([str(part) for part in command], input=stdin,
                          capture_output=True, timeout=60)
    if proc.returncode:
        raise RuntimeError(f'{proc.args!r} exited {proc.returncode}\n'
                           f'{proc.stderr.decode(errors="replace")}')
    return proc.stdout


def circuit_compare(exe, temp):
    ngspice = shutil.which('ngspice')
    if ngspice is None:
        print('UNAVAILABLE optional ngspice comparison: executable not installed')
        return
    deck = temp / 'rc.cir'
    deck.write_bytes(run(exe, '--deck'))
    log = run(ngspice, '-b', deck).decode()
    match = re.search(r'^v\(out\)\s*=\s*(\S+)', log, re.IGNORECASE | re.MULTILINE)
    assert match and abs(float(match[1]) - 6.0) < 1e-6, log
    print('PASS optional ngspice operating-point comparison')


def media_compare(exe, temp):
    ffmpeg = shutil.which('ffmpeg')
    if ffmpeg is None:
        print('UNAVAILABLE optional FFmpeg comparison: executable not installed')
        return
    pixels = run(exe, '--rgb24')
    assert len(pixels) == 12
    clip = temp / 'preview.mkv'
    flags = [ffmpeg, '-nostdin', '-hide_banner', '-loglevel', 'error']
    run(*flags, '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size', '2x2',
        '-framerate', '30000/1001', '-i', 'pipe:0', '-frames:v', '1',
        '-c:v', 'ffv1', '-threads', '1', '-f', 'matroska', clip, stdin=pixels)
    decoded = run(*flags, '-i', clip, '-frames:v', '1', '-f', 'rawvideo',
                  '-pix_fmt', 'rgb24', '-threads', '1', 'pipe:1')
    assert decoded == pixels, (pixels, decoded)
    print('PASS optional FFmpeg lossless encode/decode of the rendered RGB24 frame')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--cflag', action='append', default=[])
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    args.artifacts.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='workloads-c-', dir=args.artifacts) as directory:
        work = Path(directory).resolve()
        for name in ('circuit', 'media', 'http'):
            exe = work / name
            run(args.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                *args.cflag, '-UNDEBUG', '-I', here.parents[1] / 'kernels',
                here / f'{name}.c', '-lm', '-o', exe)
            print(run(exe).decode(), end='')
        circuit_compare(work / 'circuit', work)
        media_compare(work / 'media', work)


if __name__ == '__main__':
    main()
