"""Source and configure-time guards for the data-owned diagnostic boundary."""
import json
from pathlib import Path
import re
import subprocess
import sys


if len(sys.argv) != 4:
    raise SystemExit('usage: test_diagnostic_catalog_source.py SOURCE_ROOT WORK CXX')

root = Path(sys.argv[1]).resolve()
work = Path(sys.argv[2]).resolve()
work.mkdir(parents=True, exist_ok=True)


def text(relative):
    return (root / relative).read_text(encoding='utf-8')


def reject(pattern, paths, description, flags=0):
    expression = re.compile(pattern, flags)
    failures = []
    for relative in paths:
        payload = text(relative)
        for match in expression.finditer(payload):
            line = payload.count('\n', 0, match.start()) + 1
            failures.append(f'{relative}:{line}: {match.group(0)!r}')
    assert not failures, f'{description}:\n' + '\n'.join(failures)


diagnostic_core = [
    'include/on1x/diagnostic.hpp',
    'src/analysis.cpp',
    'src/diagnostic.cpp',
    'src/diagnostics.cpp',
    'tools/generate-diagnostic-catalog.py',
]
diagnostic_transport = [
    'stdlib/diagnostics/diagnostics.on1x',
    'stdlib/diagnostics/ppx.on1x',
    'stdppx/native/diagnostics.on1x',
]
feature_boundary = diagnostic_core + diagnostic_transport + [
    'include/on1x/ast.hpp',
    'src/cli.cpp',
    'src/parser.cpp',
    'src/ppx.cpp',
]

# The feature's identities must exist only in validated catalog data. This also
# catches a diagnostic default silently returning to the generic AST Error type.
reject(r'\bON1X-(?:DIAGNOSTIC|ANALYSIS|SOURCE|PPX-DIAGNOSTIC)'
       r'(?:-[A-Z0-9]+)*\b',
       feature_boundary, 'embedded feature diagnostic identity')
reject(r'"[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+"',
       diagnostic_core + diagnostic_transport,
       'embedded diagnostic or upstream rule code')

# Exact quoted policy values are data, while C++ enum names remain permitted as
# closed control-flow mechanics.
reject(r'"(?:note|warning|error|compiler|wetstone|ppx|off|standard|strict|'
       r'arithmetic|assertions|ownership|none|parse-token|'
       r'primitive-type-spelling|wetstone-advisory)"',
       diagnostic_core, 'embedded diagnostic level/provider/policy value')

# Guard the specific presentation text that existed before the catalog
# migration. New wording belongs in catalog.json and should not require this
# list to grow.
reject(r'(?:static-analysis work budget|PPX diagnostic protocol|'
       r'native PPX advisory|diagnostic PPX sideband|'
       r'Repair suggestions are heuristic|host validation failed|'
       r'diagnostics omitted by the registry limit|source is already valid|'
       r'ambiguous compiler-validated repairs)',
       feature_boundary, 'embedded diagnostic presentation text',
       flags=re.IGNORECASE)
reject(r'throw std::runtime_error\("(?:--(?:color|scrutiny|diagnostic|repair)|'
       r'unknown (?:diagnostic|color)|structured Syren|'
       r'duplicate --color|repair |cannot (?:write|exclusively create) repair)',
       ['src/cli.cpp'], 'embedded diagnostic command failure')
reject(r'Type::|"<eof>"',
       ['src/diagnostics.cpp'], 'embedded repair replacement policy')

ast = text('include/on1x/ast.hpp')
assert 'std::string id={}' in ast
assert not re.search(r'std::string id\s*=\s*"', ast)

stdlib_record = text('stdlib/diagnostics/diagnostics.on1x')
stdlib_ppx = text('stdlib/diagnostics/ppx.on1x')
native_ppx = text('stdppx/native/diagnostics.on1x')
assert 'message:string' not in stdlib_record
assert 'message:string' not in stdlib_ppx
assert 'basis_diagnostics_ppx_declaration()' in native_ppx
assert 'basis_diagnostics_ppx_emit(id,at)' in native_ppx

cmake = text('CMakeLists.txt')
assert 'tools/generate-diagnostic-catalog.py' in cmake
assert 'COMMAND_ERROR_IS_FATAL ANY' in cmake
assert 'file(READ "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/diagnostics/catalog.json"' not in cmake
assert not (root / 'cmake/diagnostic_catalog.hpp.in').exists()
assert not re.search(r'InvalidCatalog\([\'"]',
                     text('tools/generate-diagnostic-catalog.py'))

# Configure-time generation is independently executable and fails closed on
# duplicate keys, incomplete bootstrap roles, and free-form PPX declarations.
generator = root / 'tools/generate-diagnostic-catalog.py'
catalog_path = root / 'stdlib/diagnostics/catalog.json'
generated = work / 'diagnostic_catalog.hpp'


def generate(source, output=generated):
    return subprocess.run(
        [sys.executable, '-IB', generator, source, output],
        capture_output=True, text=True, timeout=20)


result = generate(catalog_path)
assert result.returncode == 0, result.stderr
header = generated.read_text(encoding='utf-8')
assert 'embedded_diagnostic_catalog[]' in header
assert 'embedded_diagnostic_catalog_origin[]' in header

unicode_catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
unicode_catalog['diagnostics'][0]['guidance'] = 'Unicode bootstrap 😀'
unicode_input = work / 'unicode.json'
unicode_input.write_text(json.dumps(unicode_catalog, ensure_ascii=False), encoding='utf-8')
unicode_header = work / 'unicode.hpp'
result = generate(unicode_input, unicode_header)
assert result.returncode == 0, result.stderr
probe = work / 'unicode.cpp'
probe.write_text(
    '#include "unicode.hpp"\n#include <string_view>\n'
    'static_assert(std::string_view(on1x::diagnostic::embedded_diagnostic_catalog)'
    '.find("\\360\\237\\230\\200") != std::string_view::npos);\n',
    encoding='utf-8')
result = subprocess.run([sys.argv[3], '-std=c++20', '-fsyntax-only', probe],
                        capture_output=True, text=True, timeout=20)
assert result.returncode == 0, result.stderr

duplicate = work / 'duplicate.json'
duplicate.write_text(
    '{"schema":"on1x.diagnostic-catalog/v2","schema":"duplicate"}',
    encoding='utf-8')
rejected = generate(duplicate, work / 'duplicate.hpp')
assert rejected.returncode != 0 and not rejected.stdout and not rejected.stderr

catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
catalog['diagnostics'] = [
    item for item in catalog['diagnostics'] if item['role'] != 4
]
missing_role = work / 'missing-role.json'
missing_role.write_text(json.dumps(catalog), encoding='utf-8')
assert generate(missing_role, work / 'missing-role.hpp').returncode != 0

catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
ppx = next(item for item in catalog['diagnostics'] if 'processor' in item)
ppx['free_form_message'] = True
free_form = work / 'free-form-ppx.json'
free_form.write_text(json.dumps(catalog), encoding='utf-8')
assert generate(free_form, work / 'free-form-ppx.hpp').returncode != 0

print('diagnostic catalog source and generation guards OK')
