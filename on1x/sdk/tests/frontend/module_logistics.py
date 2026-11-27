"""Run the public C SDK module test against a temporary standard package."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable = Path(sys.argv[1]).resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="sdk-module-logistics-", dir=Path.cwd()) as directory:
        root = Path(directory)
        project = root / "project"
        stdlib = root / "stdlib"
        shadow = root / "ambient" / "on1x" / "source"
        for path in (project, stdlib, shadow):
            path.mkdir(parents=True)

        (shadow / "policy.lua").write_text('error("ambient script executed")\n', encoding="utf-8")
        (stdlib / "On1xfile").write_text(
            '(On1xfile 1 (kind "stdlib") (name "std") (root "."))\n', encoding="utf-8"
        )
        (stdlib / "Tools.on1xm").write_text(
            "module std.Tools; public fn answer()->i64{return impl();}\n"
            "shared elevate(1) fn impl()->i64{return 42;}\n"
            "record Token { protected_value: i64 }\n",
            encoding="utf-8",
        )
        signature = stdlib / "Tools.on1xi"
        signature.write_text(
            "signature ToolsAPI<T> { fn answer()->T; }; "
            "module std.Tools : ToolsAPI<i64>;\n",
            encoding="utf-8",
        )
        (project / "entry.on1x").write_text(
            'import std.Tools as A; import "../stdlib/Tools.on1x";\n'
            "fn main(){assert(A.answer()==answer());}\n",
            encoding="utf-8",
        )
        # Bound the SDK diagnostic without depending on scanner token limits.
        deep = project.joinpath(*(["d" * 120] * 5))
        deep.mkdir(parents=True)
        (project / "cycle.on1x").write_text(
            f'import "{deep.relative_to(project).as_posix()}/cycle0.on1x"; fn main(){{}}\n',
            encoding="utf-8",
        )
        for index in range(33):
            (deep / f"cycle{index}.on1x").write_text(
                f'import "cycle{(index + 1) % 33}.on1x";\n',
                encoding="utf-8",
            )
        env = {
            **os.environ,
            "LIBSTONE_ASDL_PATH": str(root / "ambient"),
            "ON1X_STDLIB": str(stdlib),
            "ON1X_STDLIB_PATH": str(stdlib),
        }
        subprocess.run(
            [executable, project, stdlib, signature],
            cwd=root,
            env=env,
            check=True,
            timeout=30,
        )


if __name__ == "__main__":
    main()
