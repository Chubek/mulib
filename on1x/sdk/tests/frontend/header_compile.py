"""Compile every compiler/frontend public header independently as C11/C++20."""

from pathlib import Path
import subprocess
import sys
import tempfile


HEADERS = [
    "On1xSDK-Compiler.h",
    "On1xSDK-Session.h",
    "On1xSDK-Options.h",
    "On1xSDK-Source.h",
    "On1xSDK-SourceMap.h",
    "On1xSDK-Dependency.h",
    "On1xSDK-Compilation.h",
    "On1xSDK-Incremental.h",
    "On1xSDK-Cache.h",
    "On1xSDK-Workspace.h",
    "On1xSDK-VFS.h",
    "On1xSDK-Lexer.h",
    "On1xSDK-Token.h",
    "On1xSDK-Parser.h",
    "On1xSDK-Grammar.h",
    "On1xSDK-AST.h",
    "On1xSDK-ASTBuilder.h",
    "On1xSDK-ASTVisitor.h",
    "On1xSDK-Syntax.h",
    "On1xSDK-Attribute.h",
    "On1xSDK-Directive.h",
    "On1xSDK-Macro.h",
    "On1xSDK-Hygiene.h",
    "On1xSDK-PPX.h",
    "On1xSDK-Staging.h",
    "On1xSDK-Elaborator.h",
    "On1xSDK-Frontend.h",
]


def main():
    cc, cxx, include, build = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="headers-", dir=build) as directory:
        root = Path(directory)
        for header in HEADERS:
            for compiler, suffix, standard in (
                (cc, ".c", "c11"),
                (cxx, ".cpp", "c++20"),
            ):
                source = root / (header + suffix)
                source.write_text(
                    f'#include "{header}"\nint main(void){{return 0;}}\n',
                    encoding="utf-8",
                )
                subprocess.run(
                    [
                        compiler,
                        f"-std={standard}",
                        "-Wall",
                        "-Wextra",
                        "-Wpedantic",
                        "-Werror",
                        "-fsyntax-only",
                        "-I",
                        include,
                        str(source),
                    ],
                    check=True,
                    timeout=30,
                )
    print(f"{len(HEADERS)} headers compile independently as C11 and C++20")


if __name__ == "__main__":
    main()
