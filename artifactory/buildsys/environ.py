from __future__ import annotations

import os


def capture(keys=None):
    names = os.environ if keys is None else keys
    return {key: os.environ[key] for key in names if key in os.environ}


def merge(base, override):
    return {**(base or {}), **(override or {})}
