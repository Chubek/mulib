//go:build libglr

// Package grammar: cgo.go links dependencies/libglr when built with
// `-tags libglr`. It proves the Go engine's symbol/production model maps
// 1:1 to glr_grammar_t (see include/glr/grammar.h).
package grammar

/*
#cgo CFLAGS: -I../../dependencies/libglr/include
#cgo LDFLAGS: -L/tmp/libglr-build -lglr
#include <glr/grammar.h>
#include <glr/parser.h>
*/
import "C"
