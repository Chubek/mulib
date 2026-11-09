#!/usr/bin/env python3
"""Check raw DEFLATE, ZIP, EPUB and PDF against independent standard readers."""
import argparse
import io
from pathlib import Path
import random
import re
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--codec', required=True)
    parser.add_argument('--pdftotext')
    parser.add_argument('--pdfinfo')
    args = parser.parse_args()
    codec = str(Path(args.codec).resolve())

    def run(*arguments):
        subprocess.run([codec, *map(str, arguments)], check=True)

    temp_parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-ebooks-', dir=temp_parent) as directory:
        work = Path(directory)
        source, compressed, decoded = (work / name for name in ('source', 'compressed', 'decoded'))
        rng = random.Random(47)
        samples = [b'', b'x', bytes(range(256)), b'hello hello world\n' * 10000,
                   rng.randbytes(70000), (b'abcde' * 6554 + b'unique') * 8]
        block_types = set()
        for data in samples:
            source.write_bytes(data)
            run('encode', source, compressed)
            assert zlib.decompress(compressed.read_bytes(), -15) == data
            for level, strategy in [(0, 0), (1, 0), (6, 0), (9, 0), (6, zlib.Z_FIXED),
                                    (6, zlib.Z_HUFFMAN_ONLY), (6, zlib.Z_RLE)]:
                encoder = zlib.compressobj(level, zlib.DEFLATED, -15, 8, strategy)
                raw = encoder.compress(data) + encoder.flush()
                block_types.add((raw[0] >> 1) & 3)
                compressed.write_bytes(raw)
                run('decode', compressed, decoded)
                assert decoded.read_bytes() == data
        assert block_types == {0, 1, 2}, block_types
        # Multiple blocks, with a stored synchronization block between them.
        encoder = zlib.compressobj(wbits=-15)
        raw = encoder.compress(samples[3]) + encoder.flush(zlib.Z_SYNC_FLUSH)
        raw += encoder.compress(samples[4]) + encoder.flush()
        compressed.write_bytes(raw)
        run('decode', compressed, decoded)
        assert decoded.read_bytes() == samples[3] + samples[4]
        for malformed in [b'', b'\x07', raw[:-1], raw + b'garbage']:
            compressed.write_bytes(malformed)
            assert subprocess.run([codec, 'decode', str(compressed), str(decoded)]).returncode == 1
        # Deterministic differential mutations exercise Huffman trees, distance
        # codes and block boundaries with zlib as an independent oracle.
        encoder = zlib.compressobj(9, zlib.DEFLATED, -15)
        seed = encoder.compress(bytes(range(256)) * 4 + b'abcdef' * 200) + encoder.flush()
        for _ in range(160):
            candidate = bytearray(seed)
            for _ in range(rng.randrange(1, 4)):
                candidate[rng.randrange(len(candidate))] ^= 1 << rng.randrange(8)
            reference = zlib.decompressobj(-15)
            expected = None
            try:
                value = reference.decompress(candidate, 4 * 1024 * 1024 + 1)
                if reference.eof and not reference.unused_data and len(value) <= 4 * 1024 * 1024:
                    expected = value
            except zlib.error:
                pass
            compressed.write_bytes(candidate)
            result = subprocess.run([codec, 'decode', str(compressed), str(decoded)])
            assert result.returncode == (1 if expected is None else 0)
            if expected is not None:
                assert decoded.read_bytes() == expected

        class Streaming(io.BytesIO):
            def seekable(self):
                return False

            def seek(self, *args):
                raise io.UnsupportedOperation('streaming output')

        for streaming in (False, True):
            stream = Streaming() if streaming else io.BytesIO()
            with zipfile.ZipFile(stream, 'w') as archive:
                archive.comment = b'ZIP comment'
                archive.writestr('stored.bin', samples[2], compress_type=zipfile.ZIP_STORED)
                archive.writestr('folder/deflated.bin', samples[3], compress_type=zipfile.ZIP_DEFLATED)
            path = work / 'python.zip'
            path.write_bytes(stream.getvalue())
            for name, expected in [('stored.bin', samples[2]), ('folder/deflated.bin', samples[3])]:
                run('unzip', path, name, decoded)
                assert decoded.read_bytes() == expected
        # Optional data-descriptor signature omitted, as permitted by ZIP32.
        stream = Streaming()
        with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('entry', samples[3])
        raw = bytearray(stream.getvalue())
        eocd = len(raw) - 22
        central = struct.unpack_from('<I', raw, eocd + 16)[0]
        descriptor = central - 16
        assert raw[descriptor:descriptor+4] == b'PK\x07\x08'
        del raw[descriptor:descriptor+4]
        struct.pack_into('<I', raw, eocd - 4 + 16, central - 4)
        (work / 'bare-descriptor.zip').write_bytes(raw)
        run('unzip', work / 'bare-descriptor.zip', 'entry', decoded)
        assert decoded.read_bytes() == samples[3]

        run('emit', work)
        with zipfile.ZipFile(work / 'book.epub') as archive:
            assert archive.testzip() is None
            first = archive.infolist()[0]
            assert first.filename == 'mimetype' and first.compress_type == zipfile.ZIP_STORED
            assert archive.read(first) == b'application/epub+zip' and not first.extra
            ns = {'c': 'urn:oasis:names:tc:opendocument:xmlns:container',
                  'o': 'http://www.idpf.org/2007/opf', 'd': 'http://purl.org/dc/elements/1.1/'}
            container = ET.fromstring(archive.read('META-INF/container.xml'))
            package_path = container.find('c:rootfiles/c:rootfile', ns).attrib['full-path']
            package = ET.fromstring(archive.read(package_path))
            assert package.attrib['version'] == '3.0'
            assert package.find('o:metadata/d:title', ns).text == 'A <Book>'
            items = {item.attrib['id']: item.attrib for item in package.find('o:manifest', ns)}
            for ref in package.find('o:spine', ns):
                item = items[ref.attrib['idref']]
                assert item['media-type'] == 'application/xhtml+xml'
                ET.fromstring(archive.read('EPUB/' + item['href']))
            # Replace our fixed-Huffman compressor with zlib, preserving OCF ordering.
            with zipfile.ZipFile(work / 'repacked.epub', 'w') as repacked:
                for info in archive.infolist():
                    method = zipfile.ZIP_STORED if info.filename == 'mimetype' else zipfile.ZIP_DEFLATED
                    repacked.writestr(info.filename, archive.read(info), compress_type=method)
        run('epub', work / 'repacked.epub')
        with zipfile.ZipFile(work / 'book.epub') as archive:
            original = archive.read('EPUB/package.opf')
            malformed_packages = [
                original.replace(b'id="r1"', b'id="r0"'),
                original.replace(b'idref="r0"', b'idref="missing"'),
                original.replace(b'<manifest>', b'<manifest xmlns="urn:wrong">'),
                original.replace(b'<item id="r0"', b'<item xmlns="urn:wrong" id="r0"'),
                original.replace(b'href="chapter.xhtml"', b'href="../chapter.xhtml"'),
                original.replace(b'properties="nav"', b'properties=""'),
                original.replace(b'<dc:title>A &lt;Book&gt;</dc:title>', b'<dc:title> </dc:title>'),
                original.replace(b'unique-identifier="book-id"', b'unique-identifier="missing"'),
                original.replace(b'2026-11-09T12:30:00Z', b'2026-02-30T12:30:00Z'),
                original.replace(b'<spine>', b'<spine>unwrapped text'),
                original.replace(b'</metadata>', b'</metadata><metadata/>'),
                original.replace(b'</manifest>', b'</manifest><manifest/>'),
                original.replace(b'media-type="text/css"', b'media-type="not a media type"'),
            ]
            for package in malformed_packages:
                assert package != original
                with zipfile.ZipFile(work / 'invalid.epub', 'w') as invalid:
                    for info in archive.infolist():
                        payload = package if info.filename == 'EPUB/package.opf' else archive.read(info)
                        invalid.writestr(info.filename, payload, compress_type=info.compress_type)
                assert subprocess.run([codec, 'epub', str(work / 'invalid.epub')]).returncode == 1
            original_container = archive.read('META-INF/container.xml')
            malformed_containers = [
                original_container.replace(b'<rootfiles>', b'<rootfiles><unexpected/>'),
                original_container.replace(b'</rootfiles>', b'</rootfiles><rootfiles/>'),
            ]
            for container_bytes in malformed_containers:
                with zipfile.ZipFile(work / 'invalid.epub', 'w') as invalid:
                    for info in archive.infolist():
                        payload = container_bytes if info.filename == 'META-INF/container.xml' else archive.read(info)
                        invalid.writestr(info.filename, payload, compress_type=info.compress_type)
                assert subprocess.run([codec, 'epub', str(work / 'invalid.epub')]).returncode == 1
            # Equivalent namespace prefixes on OCF, OPF and Dublin Core must
            # resolve identically to default-namespace publications.
            rewritten = {'EPUB/package.opf': ET.tostring(ET.fromstring(original)),
                         'META-INF/container.xml': ET.tostring(ET.fromstring(original_container))}
            with zipfile.ZipFile(work / 'prefixed.epub', 'w') as prefixed:
                for info in archive.infolist():
                    prefixed.writestr(info.filename, rewritten.get(info.filename, archive.read(info)),
                                      compress_type=info.compress_type)
        run('epub', work / 'prefixed.epub')

        pdf = (work / 'book.pdf').read_bytes()
        startxref = int(re.search(rb'startxref\n(\d+)\n%%EOF\n$', pdf).group(1))
        assert pdf[startxref:startxref+5] == b'xref\n'
        lines = pdf[startxref:].splitlines()
        count = int(lines[1].split()[1])
        for number in range(1, count):
            offset = int(lines[2+number].split()[0])
            assert pdf[offset:].startswith(f'{number} 0 obj\n'.encode())
        for match in re.finditer(rb'/Length (\d+) >>\nstream\n', pdf):
            end = match.end() + int(match.group(1))
            assert pdf[end:].startswith(b'\nendstream')
        if args.pdftotext:
            text = subprocess.check_output([args.pdftotext, str(work / 'book.pdf'), '-']).decode()
            assert 'Hello (PDF) \\ world' in text and 'Second page' in text
        if args.pdfinfo:
            info = subprocess.check_output([args.pdfinfo, str(work / 'book.pdf')]).decode()
            assert re.search(r'Pages:\s+2\b', info)
    print('DEFLATE/zlib, ZIP/zipfile, EPUB/XML and PDF interoperability passed')


if __name__ == '__main__':
    main()
