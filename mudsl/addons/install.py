#!/usr/bin/env python3
"""Install Mu editor assets without modifying an editor's existing settings."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

from generate import ROOT, bundle_server


def copy_tree(source, destination):
    shutil.copytree(source, destination, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("install.sh", "build", "__pycache__"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("editor", choices=["lsp", "vim", "sublime", "jetbrains", "vscode"])
    parser.add_argument("--dest", type=Path, help="editor package directory; JetBrains: IDE configuration directory")
    parser.add_argument("--prefix", type=Path, default=Path.home() / ".local", help="LSP installation prefix")
    parser.add_argument("--checker", type=Path, help="LSP only: install this compiled mull-check alongside the server")
    parser.add_argument("--neovim", action="store_true", help="use Neovim's default package directory")
    parser.add_argument("--package-only", action="store_true", help="VS Code: build a VSIX without installing it")
    parser.add_argument("--code", default="code", help="VS Code CLI executable (e.g. codium)")
    args = parser.parse_args()
    if args.checker and (args.editor != "lsp" or not args.checker.is_file()):
        parser.error("--checker requires an existing executable and the lsp installer")
    if args.editor == "lsp":
        prefix = args.prefix.expanduser().resolve()
        destination = prefix / "share/mu-lsp"
        bundle_server(destination)
        if args.checker:
            checker = destination / ("mull-check.exe" if os.name == "nt" else "mull-check")
            shutil.copy2(args.checker, checker)
            checker.chmod(0o755)
        launcher = prefix / "bin/mull-lsp"
        launcher.parent.mkdir(parents=True, exist_ok=True)
        launcher.write_text("#!/usr/bin/env python3\nimport sys\nsys.path.insert(0, " + repr(str(destination)) + ")\nfrom server import main\nsys.exit(main())\n", encoding="utf-8")
        launcher.chmod(0o755)
        if os.name == "nt":
            launcher.with_suffix(".cmd").write_text(f'@"{sys.executable}" "{launcher}" %*\r\n', encoding="utf-8")
        print(f"Installed {launcher}; add {launcher.parent} to PATH.")
    elif args.editor == "vscode":
        directory = ROOT / "vscode"
        npm = shutil.which("npm") or "npm"
        subprocess.run([npm, "ci"], cwd=directory, check=True)
        subprocess.run([npm, "run", "package"], cwd=directory, check=True)
        vsix = directory / "mu-language-tools.vsix"
        if not args.package_only:
            subprocess.run([shutil.which(args.code) or args.code, "--install-extension", str(vsix)], check=True)
        print(f"Packaged {vsix}")
    else:
        destination = args.dest
        if destination is None and args.editor == "vim":
            data = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
            destination = data / "nvim/site/pack/mu/start/mu" if args.neovim else Path.home() / ".vim/pack/mu/start/mu"
        if destination is None and args.editor == "sublime":
            if sys.platform == "darwin":
                base = Path.home() / "Library/Application Support/Sublime Text"
            elif os.name == "nt":
                base = Path(os.environ["APPDATA"]) / "Sublime Text"
            else:
                base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "sublime-text"
            destination = base / "Packages/Mu"
        if destination is None:
            parser.error("JetBrains requires --dest pointing to the IDE configuration directory")
        destination = destination.expanduser().resolve()
        if args.editor == "jetbrains":
            copy_tree(ROOT / "jetbrains/MuLL.tmbundle", destination / "textmate/MuLL.tmbundle")
            copy_tree(ROOT / "jetbrains/templates", destination / "templates")
            print(f"Register {destination / 'textmate/MuLL.tmbundle'} in Settings > Editor > TextMate Bundles; restart the IDE for live templates.")
        else:
            copy_tree(ROOT / args.editor, destination)
        print(f"Installed {args.editor} assets in {destination}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
