class Sandbox:
    root: str
    readonly: list[str]
    writable: list[str]
    def path(self, name: str) -> str: ...
