#!/usr/bin/env python3
"""Independent Python stdlib verification of output from examples/build-publication.c."""
from pathlib import Path
import sys, zipfile, struct, zlib
import xml.etree.ElementTree as ET

if len(sys.argv) != 2:
    raise SystemExit('usage: python3 tests/verify-formats.py OUTPUT_DIRECTORY')
root = Path(sys.argv[1]); assert root.is_dir()
pdf = (root/'document.pdf').read_bytes()
assert pdf.startswith(b'%PDF-1.4') and b'/Type /Page ' in pdf
assert b'xref\n0 6\n' in pdf and pdf.endswith(b'%%EOF\n')
dvi = (root/'document.dvi').read_bytes()
assert dvi.startswith(b'\xf7\x02') and b'\xf8' in dvi and dvi[-1]==223
png = (root/'image.png').read_bytes()
assert png.startswith(b'\x89PNG\r\n\x1a\n')
width, height = struct.unpack('>II',png[16:24]); assert (width,height)==(1,1)
i=8;compressed=b''
while i<len(png):
    n=struct.unpack('>I',png[i:i+4])[0];typ=png[i+4:i+8]
    payload=png[i+8:i+8+n];crc=struct.unpack('>I',png[i+8+n:i+12+n])[0]
    assert zlib.crc32(typ+payload)==crc
    if typ==b'IDAT':compressed+=payload
    i+=n+12
assert zlib.decompress(compressed)==b'\x00\x32\x88\xc4'
with zipfile.ZipFile(root/'document.epub') as z:
    assert z.namelist()[0]=='mimetype'
    assert z.getinfo('mimetype').compress_type == zipfile.ZIP_STORED
    assert z.read('mimetype')==b'application/epub+zip'
    assert z.testzip() is None
    for f in ['META-INF/container.xml','OEBPS/content.opf','OEBPS/nav.xhtml','OEBPS/chapter.xhtml']:
        ET.fromstring(z.read(f))
pdb = (root/'document.pdb').read_bytes()
assert pdb[60:68]==b'TEXtREAd'
assert struct.unpack('>H',pdb[76:78])[0]==2
assert struct.unpack('>I',pdb[78:82])[0]==96
assert (root/'document.html').read_text().startswith('<!doctype html>')
assert '\\documentclass{article}' in (root/'document.tex').read_text()
print('Format checks passed (PDF structure, DVI framing, PNG CRC/pixels, EPUB ZIP/XML, PalmDOC and text outputs).')
