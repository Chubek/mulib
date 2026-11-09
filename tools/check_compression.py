#!/usr/bin/env python3
"""Cross-check dependency-free MuLIB codecs with independent implementations."""
import argparse
import bz2
import ctypes
import ctypes.util
import gzip
import lzma
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--codec', required=True, type=Path)
    args = parser.parse_args()
    executable = str(args.codec.resolve())
    rng = random.Random(302)
    samples = [b'', b'a', b'abc', bytes(range(256)), b'x' * 300000,
               (b'abracadabra!0123456789\x00' * 20000), rng.randbytes(140000),
               bytes(rng.randrange(5) for _ in range(100000))]
    references = {
        'gzip': (gzip.compress, gzip.decompress),
        'bzip2': (bz2.compress, bz2.decompress),
        'xz': (lzma.compress, lzma.decompress),
        'lzma': (lambda p: lzma.compress(p, format=lzma.FORMAT_ALONE),
                 lambda p: lzma.decompress(p, format=lzma.FORMAT_ALONE)),
    }
    if shutil.which('zstd'):
        references['zstd'] = (
            lambda p: subprocess.check_output(['zstd', '-q', '-c'], input=p),
            lambda p: subprocess.check_output(['zstd', '-q', '-d', '-c'], input=p))
    # lzip framing is specified independently of its LZMA-302 payload.
    filters = [{'id': lzma.FILTER_LZMA1, 'dict_size': 8388608, 'lc': 3, 'lp': 0, 'pb': 2}]

    def lzip_compress(p):
        raw = lzma.compress(p, format=lzma.FORMAT_RAW, filters=filters)
        return b'LZIP\x01\x17' + raw + struct.pack('<IQQ', zlib.crc32(p), len(p), len(raw) + 26)

    def lzip_decompress(p):
        assert p[:6] == b'LZIP\x01\x17'
        raw = lzma.decompress(p[6:-20], format=lzma.FORMAT_RAW, filters=filters)
        assert struct.unpack('<IQQ', p[-20:]) == (zlib.crc32(raw), len(raw), len(p))
        return raw

    references['lzip'] = lzip_compress, lzip_decompress
    lzf_path = ctypes.util.find_library('lzf')
    if lzf_path:
        lzf = ctypes.CDLL(lzf_path)
        for name in ['lzf_compress', 'lzf_decompress']:
            getattr(lzf, name).argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                           ctypes.c_void_p, ctypes.c_uint]
            getattr(lzf, name).restype = ctypes.c_uint

        def lzf_compress(p):
            result = b''
            for at in range(0, max(len(p), 1), 65535):
                raw = p[at:at + 65535]
                out = ctypes.create_string_buffer(len(raw) + len(raw) // 16 + 100)
                length = lzf.lzf_compress(raw, len(raw), out, len(out))
                if length and length < len(raw):
                    result += b'ZV\x01' + struct.pack('>HH', length, len(raw)) + out.raw[:length]
                else:
                    result += b'ZV\x00' + struct.pack('>H', len(raw)) + raw
            return result

        def lzf_decompress(p):
            at = 0
            result = b''
            while at < len(p):
                assert p[at:at + 2] == b'ZV'
                kind = p[at + 2]
                stored = struct.unpack('>H', p[at + 3:at + 5])[0]
                at += 5
                if kind:
                    length = struct.unpack('>H', p[at:at + 2])[0]
                    at += 2
                    out = ctypes.create_string_buffer(length)
                    assert lzf.lzf_decompress(p[at:at + stored], stored, out, length) == length
                    result += out.raw
                else:
                    result += p[at:at + stored]
                at += stored
            assert at == len(p)
            return result

        references['lzf'] = lzf_compress, lzf_decompress
    lzo_path = ctypes.util.find_library('lzo2')
    if lzo_path:
        lzo = ctypes.CDLL(lzo_path)
        for symbol in ['lzo1x_1_compress', 'lzo1x_decompress_safe']:
            function = getattr(lzo, symbol)
            function.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                                 ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p]
            function.restype = ctypes.c_int

        def lzo_compress(p):
            out = ctypes.create_string_buffer(len(p) + len(p) // 16 + 100)
            length = ctypes.c_size_t(len(out))
            work = ctypes.create_string_buffer(1048576)
            assert lzo.lzo1x_1_compress(p, len(p), out, ctypes.byref(length), work) == 0
            return out.raw[:length.value]

        def lzo_decompress(p):
            out = ctypes.create_string_buffer(16777216)
            length = ctypes.c_size_t(len(out))
            assert lzo.lzo1x_decompress_safe(p, len(p), out, ctypes.byref(length), None) == 0
            return out.raw[:length.value]

        references['lzo'] = lzo_compress, lzo_decompress

        def lzop_compress(p):
            h = bytes.fromhex('103020a009400105') + struct.pack('>IIII', 0x03000001, 0o100644, 0, 0) + b'\x00'
            stream = b'\x89LZO\x00\r\n\x1a\n' + h + struct.pack('>I', zlib.adler32(h))
            for at in range(0, len(p), 262144):
                raw = p[at:at + 262144]
                block = lzo_compress(raw)
                if len(block) >= len(raw):
                    block = raw
                stream += struct.pack('>III', len(raw), len(block), zlib.adler32(raw)) + block
            return stream + b'\x00' * 4

        def lzop_decompress(p):
            assert p[:9] == b'\x89LZO\x00\r\n\x1a\n'
            flags = struct.unpack('>I', p[17:21])[0]
            at = 34 + p[33] + 4
            raw = b''
            while True:
                length = struct.unpack('>I', p[at:at + 4])[0]
                at += 4
                if not length:
                    assert at == len(p)
                    return raw
                stored = struct.unpack('>I', p[at:at + 4])[0]
                at += 4
                ad = cd = ac = cc = None
                for flag, name in [(1, 'ad'), (0x100, 'cd'), (2, 'ac'), (0x200, 'cc')]:
                    if flags & flag and (flag not in (2, 0x200) or stored < length):
                        value = struct.unpack('>I', p[at:at + 4])[0]
                        at += 4
                        if name == 'ad': ad = value
                        if name == 'cd': cd = value
                        if name == 'ac': ac = value
                        if name == 'cc': cc = value
                block = p[at:at + stored]
                at += stored
                if ac is not None: assert zlib.adler32(block) == ac
                if cc is not None: assert zlib.crc32(block) == cc
                decoded = block if stored == length else lzo_decompress(block)
                assert len(decoded) == length
                if ad is not None: assert zlib.adler32(decoded) == ad
                if cd is not None: assert zlib.crc32(decoded) == cd
                raw += decoded

        references['lzop'] = lzop_compress, lzop_decompress
    with tempfile.TemporaryDirectory(prefix='compression-') as directory:
        root = Path(directory)

        def run(codec, operation, data, success=True, dictionary=None):
            source, dest = root / 'input', root / 'output'
            source.write_bytes(data)
            command = [executable, codec, operation, str(source), str(dest)]
            if dictionary is not None:
                path = root / 'dictionary'
                path.write_bytes(dictionary)
                command.append(str(path))
            result = subprocess.run(command, capture_output=True)
            if success:
                assert result.returncode == 0, (codec, operation, len(data), result.stderr)
                return dest.read_bytes()
            assert result.returncode != 0, (codec, 'accepted corrupt input')

        for name, (compress, decompress) in references.items():
            for data in samples:
                encoded = run(name, 'encode', data)
                assert decompress(encoded) == data, (name, 'encoder interoperability')
                external = compress(data)
                assert run(name, 'decode', external) == data, (name, 'decoder interoperability')
                for cut in {0, 1, len(external) // 2, len(external) - 1}:
                    run(name, 'decode', external[:cut], False)
                run(name, 'decode', external + b'garbage', False)
            if name not in ('lzma', 'lzo'):
                combined = compress(b'first') + compress(b'') + compress(b'second')
                assert run(name, 'decode', combined) == b'firstsecond'
            print(name + ': independent compression/decompression verified')
        if 'zstd' in references:
            for level in [1, 3, 7, 12, 19]:
                for data in samples[4:]:
                    encoded = subprocess.check_output(['zstd', '-q', '-c', '-' + str(level)], input=data)
                    assert run('zstd', 'decode', encoded) == data, ('zstd', level)
            # Exercise trained entropy tables and dictionary-prefix references.
            training = root / 'training'
            training.mkdir()
            for index in range(100):
                (training / str(index)).write_bytes(
                    (f'account={index}, record={index % 17}, compression dictionaries and entropy tables\n'.encode() * 30)
                    + rng.randbytes(128))
            dictionary_path = root / 'trained-dictionary'
            subprocess.run(['zstd', '-q', '--train', '--maxdict=2048', '-o', str(dictionary_path)]
                           + [str(p) for p in sorted(training.iterdir())], check=True, capture_output=True)
            dictionary = dictionary_path.read_bytes()
            data = (training / '11').read_bytes() + (training / '38').read_bytes()
            encoded = subprocess.check_output(['zstd', '-q', '-c', '-D', str(dictionary_path)], input=data)
            assert run('zstd', 'decode', encoded, dictionary=dictionary) == data
            run('zstd', 'decode', encoded, False)
            raw_dictionary = samples[5][-4096:]
            dictionary_path.write_bytes(raw_dictionary)
            data = raw_dictionary[-2048:] * 3
            encoded = subprocess.check_output(['zstd', '-q', '-c', '-D', str(dictionary_path)], input=data)
            assert run('zstd', 'decode', encoded, dictionary=raw_dictionary) == data
            # Many distributions/table reuse combinations, without a runtime dependency.
            for index in range(40):
                alphabet = rng.randrange(2, 257)
                data = bytes(rng.randrange(alphabet) for _ in range(rng.randrange(100, 40000)))
                encoded = subprocess.check_output(['zstd', '-q', '-c', '-' + str(index % 19 + 1)], input=data)
                assert run('zstd', 'decode', encoded) == data
        for filter_id in [lzma.FILTER_X86, lzma.FILTER_POWERPC, lzma.FILTER_IA64,
                          lzma.FILTER_ARM, lzma.FILTER_ARMTHUMB, lzma.FILTER_SPARC,
                          lzma.FILTER_DELTA]:
            for data in [rng.randbytes(200000), b'\xe8\x00\x00\x00\x00\xe9\xff\xff\xff\xff' * 20000]:
                chain = [{'id': filter_id}, {'id': lzma.FILTER_LZMA2}]
                encoded = lzma.compress(data, filters=chain)
                assert run('xz', 'decode', encoded) == data, ('xz filter', filter_id)
            if filter_id != lzma.FILTER_DELTA:
                for position in [16, 4096, 0xfffffff0]:
                    data = rng.randbytes(30000)
                    chain = [{'id': filter_id, 'start_offset': position}, {'id': lzma.FILTER_LZMA2}]
                    assert run('xz', 'decode', lzma.compress(data, filters=chain)) == data
        for check in [lzma.CHECK_NONE, lzma.CHECK_CRC32, lzma.CHECK_CRC64, lzma.CHECK_SHA256]:
            data = samples[5]
            assert run('xz', 'decode', lzma.compress(data, check=check)) == data
        for lc in range(5):
            for lp in range(5 - lc):
                for pb in [0, 2, 4]:
                    data = samples[3] * 9
                    properties = [{'id': lzma.FILTER_LZMA1, 'lc': lc, 'lp': lp,
                                   'pb': pb, 'dict_size': 4096}]
                    encoded = lzma.compress(data, format=lzma.FORMAT_ALONE, filters=properties)
                    assert run('lzma', 'decode', encoded) == data
                    # Known-size LZMA-alone may still carry an EOS marker.
                    encoded = encoded[:5] + struct.pack('<Q', len(data)) + encoded[13:]
                    assert run('lzma', 'decode', encoded) == data
        data = (bytes(range(256)) + b'abcd' * 100) * 5000
        assert run('xz', 'decode', lzma.compress(data)) == data
        for level in [1, 3, 9]:
            data = b'xxxxxyyyyyzzzz' * 20000 + bytes(range(256)) * 500
            assert run('bzip2', 'decode', bz2.compress(data, compresslevel=level)) == data


if __name__ == '__main__':
    main()
