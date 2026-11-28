import platform as _platform


def detect():
    return {
        "system": _platform.system(),
        "machine": _platform.machine(),
        "python": _platform.python_version(),
    }


def host_triplet():
    return f"{_platform.machine()}-{_platform.system().lower()}"
