"""Compile every project/tooling header independently as C11 and C++17."""

from pathlib import Path
import subprocess
import sys
import tempfile


HEADERS = (
    "On1xSDK-Project.h",
    "On1xSDK-Manifest.h",
    "On1xSDK-BuildGraph.h",
    "On1xSDK-PackageResolver.h",
    "On1xSDK-Process.h",
    "On1xSDK-Formatter.h",
    "On1xSDK-Command.h",
    "On1xSDK-CodeAction.h",
    "On1xSDK-DiagnosticCatalog.h",
    "On1xSDK-Reproducibility.h",
)


def run(command):
    subprocess.run(command, check=True, timeout=30)


def main():
    cc, cxx, include, scratch = sys.argv[1:]
    include = str(Path(include).resolve(strict=True))
    with tempfile.TemporaryDirectory(prefix="sdk-project-headers-",
                                     dir=scratch) as directory:
        root = Path(directory)
        for header in HEADERS:
            source = root / (header + ".c")
            source.write_text(
                f'#include "{header}"\n'
                "int main(void) { return 0; }\n",
                encoding="ascii",
            )
            run((cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                 "-I", include, "-c", str(source), "-o", str(source) + ".o"))
            source = root / (header + ".cpp")
            source.write_text(
                f'#include "{header}"\n'
                "int main() { return 0; }\n",
                encoding="ascii",
            )
            run((cxx, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic",
                 "-Werror", "-I", include, "-c", str(source),
                 "-o", str(source) + ".o"))
        source = root / "On1xSDK.hpp.cpp"
        source.write_text(
            '#include "On1xSDK.hpp"\n'
            "int main() { return 0; }\n",
            encoding="ascii",
        )
        run((cxx, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
             "-I", include, "-c", str(source), "-o", str(source) + ".o"))


if __name__ == "__main__":
    main()
