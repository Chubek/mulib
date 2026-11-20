#!/usr/bin/env python3
"""Cross-check MiniMPEG video with mjpegtools and libmpeg2, not libavcodec."""
import argparse
import io
from pathlib import Path
import subprocess
import tempfile

WIDTH, HEIGHT, COUNT = 64, 48, 6
Y_SIZE = WIDTH * HEIGHT
C_SIZE = Y_SIZE // 4
FRAME_SIZE = Y_SIZE + C_SIZE * 2
SEQUENCE_END = b"\0\0\1\xb7"
SEQUENCE_HEADER = b"\0\0\1\xb3"
EXTENSION = b"\0\0\1\xb5"
PICTURE = b"\0\0\1\0"


def run(argv, data=None):
    result = subprocess.run(argv, input=data, capture_output=True, timeout=20)
    if result.returncode:
        raise RuntimeError(f"{argv[0]} exited {result.returncode}: "
                           f"{result.stderr.decode(errors='replace').strip()}")
    return result.stdout


def make_frames():
    for i in range(COUNT):
        y = bytes(36 + 24 * i + 16 * (x // 32) + 8 * (row // 16)
                  for row in range(HEIGHT) for x in range(WIDTH))
        u = bytes([96 + i * 2]) * C_SIZE
        v = bytes([162 - i * 3]) * C_SIZE
        yield y + u + v


def unpack_libmpeg2(pgm_frames):
    """pgmpipe packs Y above U and V (side by side) in each grayscale PGM."""
    source = io.BytesIO(pgm_frames)
    while magic := source.readline():
        if (magic != b"P5\n"
                or source.readline().split() != [str(WIDTH).encode(), str(HEIGHT * 3 // 2).encode()]
                or source.readline().strip() != b"255"):
            raise ValueError("unexpected libmpeg2 PGM header")
        frame = source.read(FRAME_SIZE)
        if len(frame) != FRAME_SIZE:
            raise ValueError("truncated libmpeg2 frame")
        u = b"".join(frame[i:i + WIDTH // 2] for i in range(Y_SIZE, FRAME_SIZE, WIDTH))
        v = b"".join(frame[i + WIDTH // 2:i + WIDTH] for i in range(Y_SIZE, FRAME_SIZE, WIDTH))
        yield frame[:Y_SIZE] + u + v


def compare(actual, expected, label):
    if len(actual) != COUNT:
        raise ValueError(f"{label}: got {len(actual)} frames, expected {COUNT}")
    for index, (frame, wanted) in enumerate(zip(actual, expected)):
        if len(frame) != FRAME_SIZE:
            raise ValueError(f"{label}: wrong size for frame {index}")
        for start, end in ((0, Y_SIZE), (Y_SIZE, Y_SIZE + C_SIZE), (Y_SIZE + C_SIZE, FRAME_SIZE)):
            error = sum(abs(x - y) for x, y in zip(frame[start:end], wanted[start:end])) / (end - start)
            if error > 8:
                raise ValueError(f"{label}: frame {index}, plane offset {start}, mean error {error:.2f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--codec", required=True, type=Path)
    parser.add_argument("--mpeg2enc", default="mpeg2enc")
    parser.add_argument("--mpeg2dec", default="mpeg2dec")
    args = parser.parse_args()

    frames = list(make_frames())
    y4m = f"YUV4MPEG2 W{WIDTH} H{HEIGHT} F25:1 Ip A1:1 C420mpeg2\n".encode()
    y4m += b"".join(b"FRAME\n" + frame for frame in frames)
    with tempfile.TemporaryDirectory(dir=".") as directory:
        directory = Path(directory)
        raw = directory / "input.yuv"
        raw.write_bytes(b"".join(frames))
        encoded = directory / "mulib.m1v"
        run([args.codec.resolve(), "encode", raw, encoded])
        elementary = encoded.read_bytes()
        if not elementary.startswith(SEQUENCE_HEADER) or EXTENSION in elementary:
            raise ValueError("expected an MPEG-1 video elementary stream")
        if not elementary.endswith(SEQUENCE_END):
            # libmpeg2's CLI does not flush its delayed pictures at EOF alone.
            encoded.write_bytes(elementary + SEQUENCE_END)
        compare(list(unpack_libmpeg2(run([args.mpeg2dec, "-o", "pgmpipe", encoded]))),
                frames, "MiniMPEG -> libmpeg2")

        reference = directory / "mjpegtools.m1v"
        run([args.mpeg2enc, "-v", "0", "-f", "0", "-M", "1", "-n", "p",
             "-R", "1", "-b", "400", "-q", "4", "-o", reference], data=y4m)
        if not any(len(picture) >= 2 and ((picture[1] >> 3) & 7) == 3
                   for picture in reference.read_bytes().split(PICTURE)[1:]):
            raise ValueError("expected reordered B pictures from mjpegtools")
        decoded = directory / "decoded.yuv"
        run([args.codec.resolve(), "decode", reference, decoded])
        decoded_bytes = decoded.read_bytes()
        compare([decoded_bytes[i:i + FRAME_SIZE] for i in range(0, len(decoded_bytes), FRAME_SIZE)],
                frames, "mjpegtools -> MiniMPEG")
    print("MPEG-1 video: encoded for libmpeg2 and decoded a mjpegtools stream with B pictures")


if __name__ == "__main__":
    main()
