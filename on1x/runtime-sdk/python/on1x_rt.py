"""Thread-confined, synchronous On1xRT ABI-1 static-provider host."""
import ctypes as C
import os
from pathlib import Path
import sys
import threading


class Error(RuntimeError):
    def __init__(self, status, code, message):
        super().__init__(message)
        self.status = status
        self.code = code


class _Handle(C.Structure):
    _fields_ = [("id", C.c_uint64), ("owner", C.c_uint64)]


class _View(C.Structure):
    _fields_ = [("data", C.c_void_p), ("size", C.c_size_t)]


def _library(path):
    if path is None:
        name = ("libon1x_rt_sdk.1.dylib" if sys.platform == "darwin"
                else "libon1x_rt_sdk.so.1")
        path = os.environ.get("ON1X_RT_LIBRARY") or Path(__file__).resolve().parents[2] / name
    lib = C.CDLL(os.fspath(path))
    ptr, ref, h = C.c_void_p, C.POINTER(C.c_void_p), C.POINTER(_Handle)

    def bind(name, result, *args):
        fn = getattr(lib, "on1x_rt_" + name)
        fn.restype, fn.argtypes = result, args
        return fn

    if bind("abi_version", C.c_uint32)() != 1:
        raise RuntimeError("incompatible On1xRT ABI (expected 1)")
    status = C.c_uint32
    bind("engine_create", status, ptr, ref, ref)
    bind("engine_destroy", status, ref, ref)
    bind("engine_register_static_module", status, ptr, ptr, h, ref)
    bind("module_unload", status, ptr, h, ref)
    bind("instance_create", status, ptr, _Handle, h, ref)
    bind("instance_release", status, ptr, h, ref)
    bind("instance_call_i64", status, ptr, _Handle, _View, C.POINTER(C.c_int64),
         C.c_size_t, C.POINTER(C.c_int64), ref)
    bind("error_message", _View, ptr)
    bind("status_name", C.c_char_p, status)
    bind("error_code", C.c_uint32, ptr)
    bind("error_release", status, ref)
    return lib


class Host:
    """Use with `with Host() as host:`. Failed close() may be retried."""
    def __init__(self, library=None):
        self._lib = _library(library)
        self._engine = C.c_void_p()
        self._module = _Handle()
        self._instance = _Handle()
        self._provider = None
        self._thread = None

    def __copy__(self):
        raise TypeError("On1xRT owners cannot be copied")

    def __reduce_ex__(self, protocol):
        raise TypeError("On1xRT owners cannot be copied or serialized")

    def _check(self):
        if self._thread is not threading.current_thread():
            raise RuntimeError("On1xRT host is closed or belongs to another thread")

    def _call(self, operation, *args):
        err = C.c_void_p()
        status = getattr(self._lib, "on1x_rt_" + operation)(*args, C.byref(err))
        try:
            if status:
                view = self._lib.on1x_rt_error_message(err)
                message = (C.string_at(view.data, view.size) if view.size
                           else self._lib.on1x_rt_status_name(status))
                raise Error(status, self._lib.on1x_rt_error_code(err),
                            message.decode("utf-8", "replace"))
        finally:
            if err:
                self._lib.on1x_rt_error_release(C.byref(err))

    def __enter__(self):
        if self._thread is not None:
            raise RuntimeError("On1xRT host already open")
        self._thread = threading.current_thread()
        try:
            self._call("engine_create", None, C.byref(self._engine))
        except BaseException:
            self._thread = None
            raise
        return self

    def register_static(self, provider):
        """Register a trusted descriptor from an already linked/loaded provider."""
        self._check()
        if self._module.id:
            raise Error(11, 0, "host already owns a module")
        entry = provider.on1x_rt_module_descriptor_v1
        entry.argtypes, entry.restype = [], C.c_void_p
        self._call("engine_register_static_module", self._engine, entry(),
                   C.byref(self._module))
        self._provider = provider

    def instantiate(self):
        self._check()
        self._call("instance_create", self._engine, self._module, C.byref(self._instance))

    def call_i64(self, name, *args):
        self._check()
        if len(args) > 32:
            raise ValueError("On1xRT accepts at most 32 arguments")
        for arg in args:
            if type(arg) is not int:
                raise TypeError("expected an i64 argument")
            if not -(1 << 63) <= arg < (1 << 63):
                raise OverflowError("argument outside i64 range")
        encoded = name.encode("utf-8")
        view = _View(C.cast(C.c_char_p(encoded), C.c_void_p), len(encoded))
        result = C.c_int64()
        self._call("instance_call_i64", self._engine, self._instance, view,
                   (C.c_int64 * len(args))(*args), len(args), C.byref(result))
        return result.value

    def close(self):
        if self._thread is None:
            return
        self._check()
        if self._instance.id:
            self._call("instance_release", self._engine, C.byref(self._instance))
        if self._module.id:
            self._call("module_unload", self._engine, C.byref(self._module))
        self._provider = None
        self._call("engine_destroy", C.byref(self._engine))
        self._thread = None

    def __exit__(self, *_):
        self.close()
