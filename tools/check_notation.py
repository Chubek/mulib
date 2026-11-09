#!/usr/bin/env python3
"""Check independent XML/CSV/SVG readers and optional native SVG/LaTeX consumers."""
import argparse
import base64
import csv
import io
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--rsvg')
    parser.add_argument('--pdflatex')
    parser.add_argument('--lualatex')
    args = parser.parse_args()
    runtime = str(args.runtime.resolve())

    def emitted(kind):
        return subprocess.check_output([runtime, '--emit', kind], text=True, timeout=60)

    math_ns = '{http://www.w3.org/1998/Math/MathML}'
    svg_ns = '{http://www.w3.org/2000/svg}'
    equation = ET.fromstring(emitted('eqn-mathml'))
    chemistry = ET.fromstring(emitted('chem-mathml'))
    assert equation.tag == chemistry.tag == math_ns + 'math'
    assert equation.find('.//' + math_ns + 'mfrac') is not None
    assert ''.join(chemistry.itertext()).count(')') == 2
    table = ET.fromstring(emitted('table-html'))
    assert table.tag == 'table' and table.find('.//th').attrib['align'] == 'left'
    rows = list(csv.reader(io.StringIO(emitted('table-csv'))))
    assert rows == [['A & B', 'Value'], ['a < b', '42']]
    font_text = emitted('paleo-font')
    font = ET.fromstring(font_text)
    assert font.tag == svg_ns + 'svg' and len(font.findall('.//' + svg_ns + 'glyph')) == 2
    css = emitted('paleo-css')
    assert '#Paleo"' in css
    encoded = re.search(r'base64,([^"#]+)', css).group(1)
    assert ET.tostring(ET.fromstring(base64.b64decode(encoded))) == ET.tostring(font)
    encoding = emitted('paleo-encoding')
    assert len(re.findall(r'^/', encoding, re.M)) == 257 and '/triangle' in encoding

    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-notation-', dir=parent) as directory:
        work = Path(directory)
        for kind in ['chart-pie', 'chart-donut', 'chart-bar', 'chart-line', 'paleo-svg']:
            source = emitted(kind)
            assert ET.fromstring(source).tag == svg_ns + 'svg'
            if args.rsvg:
                output = subprocess.check_output([args.rsvg], input=source.encode(), timeout=60)
                assert output.startswith(b'\x89PNG\r\n\x1a\n') and len(output) > 100
        if args.pdflatex or args.lualatex:
            body = (r'\documentclass{article}\usepackage{amsmath,amssymb,Paleo}'
                    '\n' + r'\begin{document}' + '\n' + emitted('table-latex') + '\n'
                    + '\\[' + emitted('eqn-latex') + '\\]\n'
                    + '\\[' + emitted('chem-latex') + '\\]\n'
                    + emitted('paleo-latex') + '\n' + r'\end{document}' + '\n')
            (work / 'notation.tex').write_text(body)
            engines = [(args.pdflatex, 'notation.tex')]
            if args.lualatex:
                (work / 'native.tex').write_text(body.replace(emitted('paleo-latex'), emitted('paleo-native-latex')))
                engines.append((args.lualatex, 'native.tex'))
            for engine, source in engines:
                if not engine:
                    continue
                (work / 'Paleo.sty').write_text(emitted('paleo-native-package' if source == 'native.tex' else 'paleo-package'))
                assert '{0.00000001em}' in (work / 'Paleo.sty').read_text()
                result = subprocess.run([engine, '-no-shell-escape', '-interaction=nonstopmode',
                                         '-halt-on-error', source], cwd=work, capture_output=True, text=True, timeout=90)
                if result.returncode:
                    raise RuntimeError(f'{engine} failed:\n{result.stdout[-12000:]}\n{result.stderr}')
                assert (work / source.replace('.tex', '.pdf')).stat().st_size > 100
    print('Notation XML/CSV/SVG, font resources and configured SVG/LaTeX consumers: OK')


if __name__ == '__main__':
    main()
