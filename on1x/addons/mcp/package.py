#!/usr/bin/env python3
"""Operator-only deterministic addon packaging; never called by the service."""
import argparse
import json
from pathlib import Path
import re
from urllib.parse import urlsplit
import zipfile

FILES = ("README.md", "SECURITY.md", "VERIFICATION.md", "core.py", "server.py", "deploy_probe.py",
         "test_chatgpt.py", "plugin.json", "mcp.json", "package.py", "package.sh")


def package(output, endpoint):
    url = urlsplit(endpoint)
    if (url.scheme != "https" or not url.hostname or url.username or url.password
            or url.query or url.fragment or url.path != "/mcp"
            or not re.fullmatch(r"[A-Za-z0-9.-]+", url.hostname)
            or url.hostname.endswith(".invalid")):
        raise ValueError("Supply a deployed HTTPS /mcp endpoint without credentials/query/fragment.")
    home = Path(__file__).resolve().parent
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_STORED) as archive:
        for name in sorted(FILES):
            path = home / name
            if path.is_symlink() or not path.is_file():
                raise ValueError("Missing/symlinked addon package input.")
            data = path.read_bytes()
            if name == "mcp.json":
                config = json.loads(data)
                config["mcpServers"]["on1x-knowledge"]["url"] = endpoint
                data = (json.dumps(config, sort_keys=True, indent=2) + "\n").encode()
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        package(args.output, args.endpoint)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
