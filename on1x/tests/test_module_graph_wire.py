"""Compare both lossless encodings independently, including tagged empty containers."""
import json
import subprocess
import sys


def decode(text):
    pos = 0
    decoder = json.JSONDecoder()

    def atom():
        nonlocal pos
        result, pos = decoder.raw_decode(text, pos)
        assert isinstance(result, str)
        return result

    def take(token):
        nonlocal pos
        assert text.startswith(token, pos), (pos, token)
        pos += len(token)

    def node():
        if text[pos] == '"':
            return atom()
        if text.startswith('(object', pos):
            take('(object')
            result = {}
            while text[pos] != ')':
                take(' (')
                key = atom()
                assert key not in result
                take(' ')
                result[key] = node()
                take(')')
        else:
            take('(array')
            result = []
            while text[pos] != ')':
                take(' ')
                result.append(node())
        take(')')
        return result

    value = node()
    assert pos == len(text)
    return value


first = subprocess.check_output(sys.argv[1:], text=True)
assert first == subprocess.check_output(sys.argv[1:], text=True)
physical, physical_s, logical, logical_s = first.splitlines()
assert json.loads(physical) == decode(physical_s)
assert json.loads(logical) == decode(logical_s)
