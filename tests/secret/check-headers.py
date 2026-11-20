"""Check each secret header's isolated C11/C++17 include and implementation."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import re
import subprocess

INCLUDE = Path(__file__).resolve().parents[2] / "include"
LOCAL_INCLUDE = re.compile(r'^\s*#\s*include\s*"(?P<name>mulib-[^"]+\.h)"', re.M)
IMPL_SWITCH = re.compile(r'^#\s*(?:if\s+defined\s*\(\s*|ifdef\s+)(MULIB_IMPL_\w+)', re.M)
OPTIONAL = "mulib-secret-crypto-openssl.h"


def dependencies(name):
    seen, todo = set(), [name]
    while todo:
        current = todo.pop()
        if current in seen:
            continue
        seen.add(current)
        todo.extend(LOCAL_INCLUDE.findall((INCLUDE / current).read_text()))
    return sorted(seen)


def check_header(header, compiler, std):
    includes = f'#include "{header}"\n'
    source = includes
    # The optional implementation is built separately against OpenSSL::Crypto.
    if header != OPTIONAL:
        closure = dependencies(header)
        for dep in closure:
            match = IMPL_SWITCH.search((INCLUDE / dep).read_text())
            if match:
                source += "#define " + match[1] + "\n"
        source += "".join(f'#include "{dep}"\n' for dep in closure)
    source += includes
    command = [compiler, "-x", "c++" if std == "c++17" else "c", "-std=" + std,
               "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra", "-Wpedantic",
               "-Werror", "-fsyntax-only", "-I", str(INCLUDE), "-"]
    build = subprocess.run(command, input=source, text=True, capture_output=True,
                           timeout=30)
    if build.returncode:
        return f"{header} ({std}):\n{build.stderr or build.stdout}"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--cxx")
    args = parser.parse_args()
    headers = sorted(path.name for path in INCLUDE.glob("mulib-secret-*.h"))
    compilers = [(args.cc, "c11")]
    if args.cxx:
        compilers.append((args.cxx, "c++17"))
    cases = [(header, compiler, std) for header in headers for compiler, std in compilers]
    with ThreadPoolExecutor(max_workers=4) as pool:
        failures = [error for error in pool.map(lambda case: check_header(*case), cases) if error]
    if failures:
        parser.exit(1, "\n".join(failures))
    print(f"Secret kernel isolated header checks: {len(cases)} passed")


if __name__ == "__main__":
    main()
