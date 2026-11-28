package main

import (
	"bytes"
	"os"
	"path/filepath"
	"reflect"
	"testing"

	"domscript/internal/runtime"
)

func TestSplitCommandDefaultsToRunForSourceFile(t *testing.T) {
	args := []string{"domscript/examples/astar.doml"}
	command, rest := splitCommand(args)

	if command != "run" {
		t.Fatalf("command = %q, want %q", command, "run")
	}
	if !reflect.DeepEqual(rest, args) {
		t.Fatalf("args = %#v, want %#v", rest, args)
	}
}

func TestSplitCommandRecognizesCommands(t *testing.T) {
	for _, want := range []string{"run", "check", "dump-ir"} {
		t.Run(want, func(t *testing.T) {
			command, rest := splitCommand([]string{want, "source.doml"})
			if command != want {
				t.Fatalf("command = %q, want %q", command, want)
			}
			if !reflect.DeepEqual(rest, []string{"source.doml"}) {
				t.Fatalf("args = %#v, want %#v", rest, []string{"source.doml"})
			}
		})
	}
}

func TestReadSourceFromFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "source.doml")
	const source = "1 + 2"
	if err := os.WriteFile(path, []byte(source), 0o600); err != nil {
		t.Fatal(err)
	}

	got, name, err := readSource("", []string{path})
	if err != nil {
		t.Fatal(err)
	}
	if got != source || name != path {
		t.Fatalf("readSource() = (%q, %q), want (%q, %q)", got, name, source, path)
	}
}

func TestSplitCommandRecognizesCompile(t *testing.T) {
	command, rest := splitCommand([]string{"compile", "source.doml"})
	if command != "compile" || !reflect.DeepEqual(rest, []string{"source.doml"}) {
		t.Fatalf("splitCommand() = (%q, %#v)", command, rest)
	}
}

func TestWriteResultSuppressesUnit(t *testing.T) {
	var output bytes.Buffer
	writeResult(&output, runtime.Unit())
	if got := output.String(); got != "" {
		t.Fatalf("writeResult(unit) = %q, want empty output", got)
	}
}

func TestWriteResultPrintsNonUnit(t *testing.T) {
	var output bytes.Buffer
	writeResult(&output, runtime.Int(42))
	if got := output.String(); got != "42\n" {
		t.Fatalf("writeResult(int) = %q, want %q", got, "42\n")
	}
}
