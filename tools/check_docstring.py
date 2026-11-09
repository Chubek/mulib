#!/usr/bin/env python3
"""Check real ldocstr fixtures, XML/SVG and optional Pandoc interoperability."""
import argparse
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--pandoc')
    args = parser.parse_args()
    runtime = str(args.runtime.resolve())
    subprocess.run([runtime, '--fixtures', str(args.library.resolve())], check=True, timeout=60)
    xml = subprocess.check_output([runtime, '--emit', 'xml'], text=True, timeout=60)
    tree = ET.fromstring(xml)
    assert tree.tag == 'docstril' and tree.attrib['version'] == '1'
    assert len(tree.findall('.//{http://www.w3.org/2000/svg}svg')) == 2
    html = subprocess.check_output([runtime, '--emit', 'html'], text=True, timeout=60)
    assert ET.fromstring(html).tag == 'article' and '<th>' in html
    ast = subprocess.check_output([runtime, '--emit', 'pandoc'], text=True, timeout=60)
    value = json.loads(ast)
    assert value['pandoc-api-version'] == [1, 23, 1]
    tags = {node['t'] for node in value['blocks']}
    assert {'Header', 'Table', 'CodeBlock', 'BlockQuote', 'BulletList', 'OrderedList'} <= tags
    if args.pandoc:
        output = subprocess.check_output([args.pandoc, '--from=json', '--to=json'], input=ast, text=True, timeout=60)
        parsed = json.loads(output)
        assert len(parsed['blocks']) == len(value['blocks'])
        assert 'Interoperable' in subprocess.check_output(
            [args.pandoc, '--from=json', '--to=plain'], input=ast, text=True, timeout=60)
        print('DocstrIL XML/SVG and real Pandoc JSON roundtrip: OK')
    else:
        print('DocstrIL XML/SVG and Pandoc schema checks: OK (Pandoc executable not supplied)')


if __name__ == '__main__':
    main()
