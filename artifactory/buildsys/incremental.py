from __future__ import annotations

from pathlib import Path

from .core import Artifact


def is_dirty(inputs, outputs, content=False):
    input_paths = [Path(path) for path in inputs]
    output_paths = [Path(path) for path in outputs]
    if not output_paths or any(not path.exists() for path in output_paths):
        return True
    if any(not path.exists() for path in input_paths):
        return True
    oldest_output = min(path.stat().st_mtime for path in output_paths)
    if any(path.stat().st_mtime > oldest_output for path in input_paths):
        return True
    if content:
        if len(input_paths) != len(output_paths):
            return True
        return any(
            Artifact.from_path(str(input_path)).fingerprint
            != Artifact.from_path(str(output_path)).fingerprint
            for input_path, output_path in zip(input_paths, output_paths)
            if input_path.is_file() and output_path.is_file()
        )
    return False
