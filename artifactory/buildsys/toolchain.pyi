class Toolchain:
    cc: str | None
    cxx: str | None
    linker: str | None
    @classmethod
    def discover(cls) -> Toolchain: ...
