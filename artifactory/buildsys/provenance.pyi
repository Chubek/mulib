from .target import BuildAction

class ActionProvenance:
    command: tuple[str, ...]
    cwd: str | None
    env: tuple[tuple[str, str], ...]
    inputs: tuple[str, ...]
    outputs: tuple[str, ...]
    input_digests: dict[str, str]
    @classmethod
    def from_action(
        cls, action: BuildAction, *, include_content: bool = ...
    ) -> ActionProvenance: ...
    def to_dict(self) -> dict: ...
    def fingerprint(self) -> str: ...
