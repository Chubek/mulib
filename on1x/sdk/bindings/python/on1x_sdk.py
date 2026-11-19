"""Executable ctypes binding for the implemented On1x SDK project subset.

The binding owns every native allocation through callbacks into the process C
allocator.  Set ``ON1X_SDK_PROJECT_LIBRARY`` to an explicit installed shared
library when it is not discoverable as ``On1xSDKProject``.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
from pathlib import Path
from typing import Optional

ABI = 2
OK = 0
MAX_PATH = 1024


class SDKError(RuntimeError):
    def __init__(self, status: int, operation: str, diagnostic: str = ""):
        detail = f": {diagnostic}" if diagnostic else ""
        super().__init__(f"{operation} failed with On1x SDK status {status}{detail}")
        self.status = status
        self.operation = operation
        self.diagnostic = diagnostic


class String(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("size", ctypes.c_size_t)]

    @classmethod
    def from_bytes(cls, value: bytes):
        if not value:
            return cls(None, 0), None
        storage = ctypes.create_string_buffer(value)
        return cls(ctypes.addressof(storage), len(value)), storage

    def bytes(self) -> bytes:
        return ctypes.string_at(self.data, self.size) if self.size else b""


class Bytes(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("size", ctypes.c_size_t)]

    @classmethod
    def from_bytes(cls, value: bytes):
        if not value:
            return cls(None, 0), None
        storage = ctypes.create_string_buffer(value)
        return cls(ctypes.addressof(storage), len(value)), storage


ALLOCATE = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p,
                            ctypes.c_size_t, ctypes.c_size_t)
REALLOCATE = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                              ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t)
RELEASE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p,
                           ctypes.c_size_t, ctypes.c_size_t)


class Allocator(ctypes.Structure):
    _fields_ = [
        ("abi_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("context", ctypes.c_void_p),
        ("allocate", ALLOCATE),
        ("reallocate", REALLOCATE),
        ("release", RELEASE),
    ]


class Span(ctypes.Structure):
    _fields_ = [
        ("abi_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("source_revision", ctypes.c_ubyte * 32),
        ("begin", ctypes.c_size_t),
        ("end", ctypes.c_size_t),
    ]


class Diagnostic(ctypes.Structure):
    pass


Diagnostic._fields_ = [
    ("abi_version", ctypes.c_uint32),
    ("struct_size", ctypes.c_uint32),
    ("code", String),
    ("severity", ctypes.c_uint32),
    ("message", String),
    ("primary", Span),
    ("related_count", ctypes.c_size_t),
    ("related", ctypes.POINTER(Span)),
]

DIAGNOSTIC = ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.c_void_p,
                              ctypes.POINTER(Diagnostic))


class DiagnosticSink(ctypes.Structure):
    _fields_ = [
        ("abi_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("context", ctypes.c_void_p),
        ("emit", DIAGNOSTIC),
    ]


class ManifestInfo(ctypes.Structure):
    _fields_ = [
        ("abi_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("kind", ctypes.c_uint32),
        ("name", String),
        ("directory", String),
        ("source_revision", ctypes.c_ubyte * 32),
    ]


class ManifestTarget(ctypes.Structure):
    _fields_ = [
        ("abi_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("name", String),
        ("source", String),
    ]


class ManifestDependency(ctypes.Structure):
    _fields_ = [
        ("abi_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("name", String),
        ("kind", ctypes.c_uint32),
        ("source", String),
        ("selector_kind", ctypes.c_uint32),
        ("selector", String),
    ]


_libc = ctypes.CDLL(None)
if not hasattr(_libc, "posix_memalign"):
    raise ImportError("the On1x SDK Python binding currently requires POSIX aligned allocation")
_libc.posix_memalign.argtypes = [ctypes.POINTER(ctypes.c_void_p),
                                 ctypes.c_size_t, ctypes.c_size_t]
_libc.posix_memalign.restype = ctypes.c_int
_libc.free.argtypes = [ctypes.c_void_p]
_libc.free.restype = None


@ALLOCATE
def _allocate(_context, size, alignment):
    pointer = ctypes.c_void_p()
    if _libc.posix_memalign(ctypes.byref(pointer), alignment, max(1, size)):
        return None
    return pointer.value


@REALLOCATE
def _reallocate(_context, _pointer, _old_size, _new_size, _alignment):
    # The implemented C subset never requests reallocation.
    return None


@RELEASE
def _release(_context, pointer, _size, _alignment):
    _libc.free(pointer)


_allocator = Allocator(ABI, ctypes.sizeof(Allocator), None,
                       _allocate, _reallocate, _release)


def _library_path() -> str:
    explicit = os.environ.get("ON1X_SDK_PROJECT_LIBRARY")
    if explicit:
        path = Path(explicit).expanduser().resolve(strict=True)
        if not path.is_file():
            raise ImportError(f"On1x SDK project library is not a file: {path}")
        return str(path)
    beside = Path(__file__).resolve().parent
    for directory in (beside, *tuple(beside.parents)[:3]):
        candidate = directory / "libOn1xSDKProject.so"
        if candidate.is_file():
            return str(candidate.resolve(strict=True))
    found = ctypes.util.find_library("On1xSDKProject")
    if found:
        return found
    raise ImportError("libOn1xSDKProject not found; set ON1X_SDK_PROJECT_LIBRARY")


_library = ctypes.CDLL(_library_path())
_library.on1x_sdk_manifest_parse.argtypes = [
    Bytes, ctypes.c_void_p, ctypes.POINTER(Allocator),
    ctypes.POINTER(DiagnosticSink), ctypes.POINTER(ctypes.c_void_p),
]
_library.on1x_sdk_manifest_parse.restype = ctypes.c_uint32
_library.on1x_sdk_manifest_info.argtypes = [ctypes.c_void_p]
_library.on1x_sdk_manifest_info.restype = ctypes.POINTER(ManifestInfo)
_library.on1x_sdk_manifest_target_count.argtypes = [ctypes.c_void_p]
_library.on1x_sdk_manifest_target_count.restype = ctypes.c_size_t
_library.on1x_sdk_manifest_target_at.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
_library.on1x_sdk_manifest_target_at.restype = ctypes.POINTER(ManifestTarget)
_library.on1x_sdk_manifest_dependency_count.argtypes = [ctypes.c_void_p]
_library.on1x_sdk_manifest_dependency_count.restype = ctypes.c_size_t
_library.on1x_sdk_manifest_dependency_at.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
_library.on1x_sdk_manifest_dependency_at.restype = ctypes.POINTER(ManifestDependency)
_library.on1x_sdk_manifest_destroy.argtypes = [ctypes.c_void_p]
_library.on1x_sdk_manifest_destroy.restype = None
_library.on1x_sdk_project_normalize_path.argtypes = [
    String, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
_library.on1x_sdk_project_normalize_path.restype = ctypes.c_uint32


class Manifest:
    """Move-like native manifest owner with explicit/context-manager cleanup."""

    def __init__(self, source: str | bytes):
        payload = source.encode("utf-8") if isinstance(source, str) else bytes(source)
        raw, storage = Bytes.from_bytes(payload)
        handle = ctypes.c_void_p()
        messages = []

        @DIAGNOSTIC
        def diagnostic(_context, record):
            value = record.contents
            messages.append(f"{value.code.bytes().decode('ascii', 'replace')}: "
                            f"{value.message.bytes().decode('utf-8', 'replace')}")
            return OK

        sink = DiagnosticSink(ABI, ctypes.sizeof(DiagnosticSink), None, diagnostic)
        status = int(_library.on1x_sdk_manifest_parse(
            raw, None, ctypes.byref(_allocator), ctypes.byref(sink),
            ctypes.byref(handle)))
        # Keep callback/input objects live through the native call.
        del storage, sink, diagnostic
        if status != OK:
            raise SDKError(status, "manifest parse", "; ".join(messages))
        self._handle: Optional[int] = handle.value

    def __copy__(self):
        raise TypeError("native On1x SDK manifest owners cannot be copied")

    def __deepcopy__(self, _memo):
        raise TypeError("native On1x SDK manifest owners cannot be copied")

    def __reduce_ex__(self, _protocol):
        raise TypeError("native On1x SDK manifest owners cannot be serialized")

    def close(self):
        handle, self._handle = self._handle, None
        if handle:
            _library.on1x_sdk_manifest_destroy(handle)

    def __enter__(self):
        if not self._handle:
            raise RuntimeError("manifest is closed")
        return self

    def __exit__(self, _kind, _value, _traceback):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            # Module globals may already be cleared during interpreter shutdown.
            pass

    def _info(self) -> ManifestInfo:
        if not self._handle:
            raise RuntimeError("manifest is closed")
        pointer = _library.on1x_sdk_manifest_info(self._handle)
        if not pointer:
            raise RuntimeError("native manifest handle is invalid")
        return pointer.contents

    @property
    def name(self) -> str:
        return self._info().name.bytes().decode("utf-8")

    @property
    def directory(self) -> str:
        return self._info().directory.bytes().decode("utf-8")

    @property
    def kind(self) -> int:
        return int(self._info().kind)

    @property
    def source_revision(self) -> str:
        return bytes(self._info().source_revision).hex()

    @property
    def targets(self):
        if not self._handle:
            raise RuntimeError("manifest is closed")
        result = []
        for index in range(_library.on1x_sdk_manifest_target_count(self._handle)):
            item = _library.on1x_sdk_manifest_target_at(self._handle, index).contents
            result.append((item.name.bytes().decode("utf-8"),
                           item.source.bytes().decode("utf-8")))
        return tuple(result)

    @property
    def dependencies(self):
        if not self._handle:
            raise RuntimeError("manifest is closed")
        result = []
        for index in range(_library.on1x_sdk_manifest_dependency_count(self._handle)):
            item = _library.on1x_sdk_manifest_dependency_at(self._handle, index).contents
            result.append({
                "name": item.name.bytes().decode("utf-8"),
                "kind": int(item.kind),
                "source": item.source.bytes().decode("utf-8"),
                "selector_kind": int(item.selector_kind),
                "selector": item.selector.bytes().decode("utf-8"),
            })
        return tuple(result)


def normalize_path(value: str, *, allow_root_dot: bool = False) -> str:
    encoded = value.encode("utf-8")
    source, storage = String.from_bytes(encoded)
    output = ctypes.create_string_buffer(MAX_PATH)
    size = ctypes.c_size_t()
    status = int(_library.on1x_sdk_project_normalize_path(
        source, int(allow_root_dot), output, len(output), ctypes.byref(size)))
    del storage
    if status != OK:
        raise SDKError(status, "path normalization")
    return bytes(output.raw[:size.value]).decode("utf-8")


__all__ = ["ABI", "Manifest", "SDKError", "normalize_path"]
