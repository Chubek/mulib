package main

import (
	"flag"
	"fmt"
	"io"
	"os"
	stdexec "os/exec"
	"path/filepath"
	"strings"

	"domscript/internal/diag"
	"domscript/internal/exec"
	"domscript/internal/ir"
	"domscript/internal/lexer"
	"domscript/internal/packagefmt"
	"domscript/internal/parser"
	"domscript/internal/runtime"
	"domscript/internal/sema"
	"domscript/internal/transpile"
	"domscript/stdlib"
)

func usage() {
	fmt.Fprintln(os.Stderr, "usage: dsc [run|compile|check|dump-ir] [-e source] [-T target] [-o output] [-E] [-n] [-s] [-a] [-X] [file]")
}

func main() {
	command, args := splitCommand(os.Args[1:])
	fs := flag.NewFlagSet("dsc", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	expr := fs.String("e", "", "evaluate source text")
	noOptimize := fs.Bool("no-optimize", false, "disable bytecode optimization")
	native := fs.Bool("native", false, "emit native package payload")
	static := fs.Bool("static", false, "include libraries in package")
	archive := fs.Bool("archive", false, "treat input as archive")
	xst := fs.Bool("X", false, "enable extensible syntax tree parsing")
	target := fs.String("T", "", "transpile target (js, c, qbe)")
	outputPath := fs.String("o", "", "transpiler output path")
	execAfter := fs.Bool("exec-after", false, "execute transpiled output after writing it")
	fs.BoolVar(execAfter, "E", false, "execute transpiled output after writing it")
	interactive := fs.Bool("interactive", false, "start interactive REPL")
	replConfig := fs.String("repl-config", "", "REPL INI configuration path")
	if err := fs.Parse(args); err != nil {
		os.Exit(2)
	}
	if *interactive || (len(os.Args) == 1) {
		repl(*replConfig, *xst)
		return
	}
	src, name, err := readSource(*expr, fs.Args())
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	if *target != "" {
		if *target != "js" && *target != "c" && *target != "qbe" {
			fmt.Fprintf(os.Stderr, "unsupported transpile target %q (supported: js, c, qbe)\n", *target)
			os.Exit(2)
		}
		var output string
		var ext string
		if *target == "js" {
			output, err = transpile.JavaScriptWithOptions(src, name, parser.ParseOptions{XST: *xst})
			ext = ".js"
		} else if *target == "c" {
			output, err = transpile.CWithOptions(src, name, parser.ParseOptions{XST: *xst})
			ext = ".c"
		} else {
			output, err = transpile.QBEWithOptions(src, name, parser.ParseOptions{XST: *xst})
			ext = ".qbe"
		}
		e := err
		if e != nil {
			fmt.Fprintln(os.Stderr, e)
			os.Exit(1)
		}
		out := *outputPath
		if out == "" {
			out = strings.TrimSuffix(name, filepath.Ext(name)) + ext
		}
		if name == "<cmdline>" && !*execAfter && *outputPath == "" {
			fmt.Print(output)
			return
		}
		if e = os.WriteFile(out, []byte(output), 0644); e != nil {
			fmt.Fprintln(os.Stderr, e)
			os.Exit(1)
		}
		fmt.Println(out)
		if *execAfter {
			if e = executeTranspiled(*target, out); e != nil {
				fmt.Fprintln(os.Stderr, e)
				os.Exit(1)
			}
		}
		return
	}
	switch command {
	case "run":
		if *archive && filepath.Ext(name) != ".dsca" {
			fmt.Fprintln(os.Stderr, "--archive requires a .dsca input")
			os.Exit(2)
		}
		run(src, name, *xst)
	case "compile":
		if filepath.Ext(name) == ".dsca" {
			fmt.Fprintln(os.Stderr, "compile expects a .doml source file")
			os.Exit(2)
		}
		f, ok := compile(src, name, !*noOptimize, *xst)
		if !ok {
			os.Exit(1)
		}
		_ = f
		out := strings.TrimSuffix(name, filepath.Ext(name)) + ".dsca"
		payload := []byte(src)
		b, e := packagefmt.Build(payload, *native, *static, nil, nil)
		if e != nil {
			fmt.Fprintln(os.Stderr, e)
			os.Exit(1)
		}
		if e = os.WriteFile(out, b, 0644); e != nil {
			fmt.Fprintln(os.Stderr, e)
			os.Exit(1)
		}
		fmt.Println(out)
	case "check":
		_, ok := compile(src, name, false, *xst)
		if !ok {
			os.Exit(1)
		}
	case "dump-ir":
		f, ok := compile(src, name, !*noOptimize, *xst)
		if !ok {
			os.Exit(1)
		}
		fmt.Print(f.Disassemble())
	default:
		usage()
		os.Exit(2)
	}
}

func executeTranspiled(target, sourcePath string) error {
	switch target {
	case "js":
		node, err := stdexec.LookPath("node")
		if err != nil {
			return fmt.Errorf("cannot execute JavaScript: node not found: %w", err)
		}
		cmd := stdexec.Command(node, sourcePath)
		cmd.Stdin, cmd.Stdout, cmd.Stderr = os.Stdin, os.Stdout, os.Stderr
		return cmd.Run()
	case "c":
		cc := os.Getenv("CC")
		if cc == "" {
			cc = "cc"
		}
		dir, err := os.MkdirTemp("", "domscript-c-*")
		if err != nil {
			return err
		}
		defer os.RemoveAll(dir)
		exe := filepath.Join(dir, "domscript.out")
		if err := runTool(cc, sourcePath, "-o", exe); err != nil {
			return fmt.Errorf("compile generated C: %w", err)
		}
		return runTool(exe)
	case "qbe":
		qbe, err := qbeTool()
		if err != nil {
			return err
		}
		cc := os.Getenv("CC")
		if cc == "" {
			cc = "cc"
		}
		dir, err := os.MkdirTemp("", "domscript-qbe-*")
		if err != nil {
			return err
		}
		defer os.RemoveAll(dir)
		asm := filepath.Join(dir, "domscript.s")
		exe := filepath.Join(dir, "domscript.out")
		if err := runTool(qbe, "-o", asm, sourcePath); err != nil {
			return fmt.Errorf("compile generated QBE: %w", err)
		}
		if err := runTool(cc, asm, "-o", exe); err != nil {
			return fmt.Errorf("assemble generated QBE: %w", err)
		}
		return runTool(exe)
	default:
		return fmt.Errorf("cannot execute transpile target %q", target)
	}
}

func runTool(name string, args ...string) error {
	cmd := stdexec.Command(name, args...)
	cmd.Stdin, cmd.Stdout, cmd.Stderr = os.Stdin, os.Stdout, os.Stderr
	return cmd.Run()
}

func qbeTool() (string, error) {
	if configured := os.Getenv("QBE"); configured != "" {
		if _, err := os.Stat(configured); err == nil {
			return configured, nil
		}
	}
	if path, err := stdexec.LookPath("qbe"); err == nil {
		return path, nil
	}
	for _, start := range []string{workingDir(), executableDir()} {
		for dir := start; dir != ""; dir = filepath.Dir(dir) {
			candidate := filepath.Join(dir, "third_party", "qbe", "qbe")
			if _, err := os.Stat(candidate); err == nil {
				return candidate, nil
			}
			parent := filepath.Dir(dir)
			if parent == dir {
				break
			}
		}
	}
	return "", fmt.Errorf("cannot execute QBE: qbe binary not found (build third_party/qbe or set QBE)")
}

func workingDir() string {
	dir, err := os.Getwd()
	if err != nil {
		return ""
	}
	return dir
}

func executableDir() string {
	path, err := os.Executable()
	if err != nil {
		return ""
	}
	return filepath.Dir(path)
}

func splitCommand(args []string) (string, []string) {
	if len(args) > 0 {
		switch args[0] {
		case "run", "compile", "check", "dump-ir":
			return args[0], args[1:]
		}
	}
	return "run", args
}

func readSource(expr string, files []string) (string, string, error) {
	if expr != "" {
		return expr, "<cmdline>", nil
	}
	if len(files) != 1 {
		return "", "", fmt.Errorf("expected one source file or -e")
	}
	b, err := os.ReadFile(files[0])
	if filepath.Ext(files[0]) == ".dsca" {
		p, e := packagefmt.Read(files[0])
		if e != nil {
			return "", "", e
		}
		return string(p.Code), files[0], nil
	}
	return string(b), files[0], err
}

func compile(src, name string, optimize bool, xst bool) (*ir.Func, bool) {
	diags := diag.NewSet(name, src)
	_ = lexer.Scan(src, name, diags)
	prog := parser.Parse(src, name, diags, parser.ParseOptions{XST: xst})
	if !diags.HasErrors() {
		st := runtime.NewState()
		st.XST = xst
		stdlib.RegisterAll(st)
		if err := exec.ResolveImports(st, prog, name); err != nil {
			fmt.Fprintln(os.Stderr, err)
			return nil, false
		}
		info := sema.Check(prog, st.RegisteredNames(), st.ChunkGlobalNames(), diags)
		if !diags.HasErrors() {
			f, err := exec.Compile(prog, info, st, optimize)
			if err == nil {
				if err = ir.Validate(f); err == nil {
					return f, true
				}
			}
			if err != nil {
				fmt.Fprintln(os.Stderr, err)
			}
		}
	}
	if diags.HasErrors() {
		fmt.Fprintln(os.Stderr, diags.RenderAll())
	}
	return nil, false
}

func run(src, name string, xst bool) {
	st := runtime.NewState()
	st.XST = xst
	stdlib.RegisterAll(st)
	st.Output = os.Stdout
	v, err := exec.EvalChunk(st, src, name)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	writeResult(os.Stdout, v)
}

func writeResult(w io.Writer, v runtime.Value) {
	if v.Kind() == runtime.KindUnit {
		return
	}
	fmt.Fprintln(w, formatValue(v))
}

func formatValue(v runtime.Value) string {
	switch v.Kind() {
	case runtime.KindUnit:
		return "()"
	case runtime.KindBool:
		return fmt.Sprint(v.BoolValue())
	case runtime.KindInt:
		return fmt.Sprint(v.IntValue())
	case runtime.KindFloat:
		return fmt.Sprint(v.FloatValue())
	case runtime.KindString:
		return v.StrValue()
	case runtime.KindTag:
		return ":" + v.StrValue()
	case runtime.KindList:
		parts := make([]string, len(v.List().Elems))
		for i, e := range v.List().Elems {
			parts[i] = formatValue(e)
		}
		if tag := v.ListTag(); tag != "" {
			return ":" + tag + "[" + strings.Join(parts, ", ") + "]"
		}
		return "[" + strings.Join(parts, ", ") + "]"
	default:
		return v.Kind().String()
	}
}
