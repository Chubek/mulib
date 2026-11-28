package asdl

import (
	"domscript/internal/runtime"
)

// Register installs std.asdl natives (currently none; absyn values are
// plain tagged lists, e.g. `:Bool[true]`, so no runtime help is needed).
func Register(st *runtime.State) {
	_ = st
}
