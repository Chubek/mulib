from __future__ import annotations

import shutil
from dataclasses import dataclass, field


@dataclass
class Toolchain:
    cc: str | None = None
    cxx: str | None = None
    linker: str | None = None
    tools: dict[str, str] = field(default_factory=dict)

    @classmethod
    def discover(cls):
        cc = shutil.which("cc") or shutil.which("gcc")
        cxx = shutil.which("c++") or shutil.which("g++")
        linker = shutil.which("ld") or shutil.which("lld") or shutil.which("ld.lld")
        return cls(cc, cxx, linker)
