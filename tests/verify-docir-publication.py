#!/usr/bin/env python3
"""Independent structural readers and optional real LaTeX/PDF validation; not certification."""
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

if len(sys.argv) != 2:
    raise SystemExit('usage: verify-docir-publication.py OUTPUT_DIRECTORY')
root = Path(sys.argv[1]).resolve()
xhtml = '{http://www.w3.org/1999/xhtml}'
math = '{http://www.w3.org/1998/Math/MathML}'
html = ET.fromstring((root / 'document.html').read_bytes())
assert html.tag == xhtml + 'html'
assert html.find('.//' + math + 'mfrac') is not None
assert html.find('.//' + xhtml + 'script') is None
ids = [node.attrib['id'] for node in html.iter() if 'id' in node.attrib]
assert len(ids) == len(set(ids)) and 'overview' in ids

data = (root / 'document.docir').read_bytes()
assert data[:8] == b'MuDocIR\x01'
count, = struct.unpack_from('>I', data, 8)
assert count > 1
position = 12
for index in range(count):
    kind, parent, level, flags, font, indent, begin, end, line, column = struct.unpack_from('>IIIIiiQQQQ', data, position)
    position += 56
    assert 0 <= kind <= 23 and 1 <= level <= 6 and not flags & ~7
    assert parent < index if index else parent == 0xffffffff
    values = []
    for _ in range(4):
        length, = struct.unpack_from('>I', data, position)
        position += 4
        if length == 0xffffffff:
            values.append(None)
        else:
            assert length <= len(data) - position
            value = data[position:position + length]
            assert b'\0' not in value
            values.append(value.decode('utf-8'))
            position += length
    if values[3] is not None:
        assert begin <= end and line >= 1 and column >= 1
assert position == len(data)

with zipfile.ZipFile(root / 'document.epub') as archive:
    assert archive.namelist()[0] == 'mimetype'
    assert archive.read('mimetype') == b'application/epub+zip'
    assert archive.getinfo('mimetype').compress_type == zipfile.ZIP_STORED
    assert archive.testzip() is None
    opf = ET.fromstring(archive.read('EPUB/package.opf'))
    ns = {'p': 'http://www.idpf.org/2007/opf'}
    items = {item.attrib['id']: item for item in opf.find('p:manifest', ns)}
    spine = opf.find('p:spine', ns)
    assert len(spine) == 2
    for reference in spine:
        item = items[reference.attrib['idref']]
        assert 'mathml' in item.attrib.get('properties', '').split()
        chapter = ET.fromstring(archive.read('EPUB/' + item.attrib['href']))
        assert chapter.tag == xhtml + 'html' and chapter.find('.//' + math + 'mfrac') is not None

tex = (root / 'document.tex').read_text()
assert r'\frac{a}{b}' in tex and r'\input{not-executed}' not in tex
assert r'\begin{thebibliography}' in tex
engine = shutil.which('pdflatex')
if engine:
    for _ in range(2):
        result = subprocess.run([engine, '-interaction=nonstopmode', '-halt-on-error', '-no-shell-escape',
                                 'document.tex'], cwd=root, capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise SystemExit('LaTeX validation failed:\n' + (result.stdout + result.stderr)[-4000:])
    checker = shutil.which('qpdf')
    if checker:
        subprocess.run([checker, '--check', str(root / 'document.pdf')], capture_output=True, check=True, timeout=30)
        qdf = subprocess.check_output([checker, '--qdf', '--object-streams=disable', str(root / 'document.pdf'), '-'], timeout=30)
        assert b'https://example.test/a_b?x=100%25&y=2#part' in qdf
    extractor = shutil.which('pdftotext')
    if extractor:
        text = subprocess.check_output([extractor, str(root / 'document.pdf'), '-'], text=True, timeout=30)
        assert 'Semantic publication' in text and 'not-executed' in text
    print('Real LaTeX/PDF checks passed (shell escape disabled).')
else:
    print('SKIP: pdflatex unavailable; LaTeX source checked structurally only.')
print('DocIR v1, HTML/XHTML/MathML and EPUB structural checks passed; no standards certification claimed.')
