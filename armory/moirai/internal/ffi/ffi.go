// Package ffi is the single registration point for native functions,
// shared by the Go API, the C bridge, and the stdlib native halves.
// It owns the mapping from names to callables, the argument/result
// marshaling conventions, and the async-token allocation used by
// RegisterAsync / ResolveAsync on both surfaces.
package ffi

import "domscript/internal/runtime"

// GoNative is a synchronous native function: it receives the state and
// arguments, and returns a value or an error. The error yields :Error at
// the caller's ~ boundary.
type GoNative = runtime.GoNative

// GoAsyncStart begins an asynchronous host operation and returns a token
// immediately. The suspended caller resumes via ResolveAsync.
type GoAsyncStart = runtime.GoAsyncStart

// Register binds a synchronous native under name in the state.
func Register(st *runtime.State, name string, fn GoNative) {
	st.RegisterNative(name, fn)
}

// RegisterAsync binds an asynchronous native under name.
func RegisterAsync(st *runtime.State, name string, start GoAsyncStart) {
	st.RegisterAsyncNative(name, start)
}

// ResolveAsync completes an async operation, binding the result to the
// token the host obtained from the async start function.
func ResolveAsync(st *runtime.State, token uint64, result runtime.Value, err error) {
	st.ResolveAsync(token, result, err)
}

// NewAsyncToken allocates a fresh token for an async operation.
func NewAsyncToken(st *runtime.State) uint64 {
	return st.NewAsyncToken()
}
