from .core import digest


def compare(expected, actual):
    return digest(expected) == digest(actual)


def reproducibility_key(command, inputs=()):
    return digest({"command": command, "inputs": tuple(inputs)})
