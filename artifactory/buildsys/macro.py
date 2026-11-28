from __future__ import annotations

import re


def expand_macros(text, macros):
    if not isinstance(text, str):
        raise TypeError("macro input must be text")
    values = {str(name): str(value) for name, value in macros.items()}
    pattern = re.compile(r"\$\(([^(){}]+)\)|\$\{([^${}]+)\}")

    def replace(match):
        name = match.group(1) or match.group(2)
        return values.get(name, match.group(0))

    return pattern.sub(replace, text)
