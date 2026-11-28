//go:build !keystone || !amd64 || !linux || !cgo

package jit

func buildNativeTrace(t Trace, isa ISA) (uintptr, []byte, bool, error) {
	return 0, nil, false, nil
}

func releaseExecutable(buf []byte) error {
	return nil
}

func CallNativeTrace(entry uintptr, localsPtr, stackPtr uintptr, sp uint64) (spOut uint64, nextIP uint64, exitKind uint64, ret uint64, ok bool, err error) {
	return 0, 0, 0, 0, false, nil
}
