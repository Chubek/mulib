package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	shellExec "os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"unicode"

	"domscript/internal/exec"
	"domscript/internal/runtime"
	"domscript/stdlib"
)

var directives = []string{"%load", "%library", "%theme", "%jit", "%aot", "%save", "%restore", "%shell", "%output", "%autocomplete", "%highlight", "%config", "%quit"}

var domscriptKeywords = []string{
	"and", "as", "break", "const", "continue", "else", "enum", "false",
	"fn", "for", "form", "if", "import", "in", "let", "match", "module",
	"not", "on", "or", "program", "return", "true", "while",
}

type replConfig struct {
	Theme        string
	AutoComplete bool
	Highlight    bool
	History      string
}

type replState struct {
	vm         *runtime.State
	cfg        replConfig
	configPath string
	jit, aot   bool
	output     io.Writer
	editor     replEditor
}

type replEditor interface {
	ReadLine(prompt string) (string, bool)
	AddHistory(line string)
	SetAutocomplete(enabled bool)
	SetHighlight(enabled bool)
	SetTheme(theme string)
	Close()
	Prompt() string
}

func defaultREPLConfig() string {
	if p := os.Getenv("XDG_CONFIG_HOME"); p != "" {
		return filepath.Join(p, "domscript", "REPL.ini")
	}
	h, _ := os.UserHomeDir()
	return filepath.Join(h, ".config", "domscript", "REPL.ini")
}

func loadREPLConfig(path string) replConfig {
	c := replConfig{AutoComplete: true, Highlight: true}
	f, e := os.Open(path)
	if e != nil {
		return c
	}
	defer f.Close()
	s := bufio.NewScanner(f)
	for s.Scan() {
		p := strings.SplitN(strings.TrimSpace(s.Text()), "=", 2)
		if len(p) != 2 || strings.HasPrefix(p[0], "#") {
			continue
		}
		switch strings.ToLower(p[0]) {
		case "theme":
			c.Theme = p[1]
		case "autocomplete":
			c.AutoComplete, _ = strconv.ParseBool(p[1])
		case "highlight":
			c.Highlight, _ = strconv.ParseBool(p[1])
		case "history":
			c.History = p[1]
		}
	}
	return c
}

func saveREPLConfig(path string, c replConfig) error {
	if e := os.MkdirAll(filepath.Dir(path), 0700); e != nil {
		return e
	}
	return os.WriteFile(path, []byte(fmt.Sprintf("theme=%s\nautocomplete=%t\nhighlight=%t\nhistory=%s\n", c.Theme, c.AutoComplete, c.Highlight, c.History)), 0600)
}

func repl(configPath string, xst bool) {
	if configPath == "" {
		configPath = defaultREPLConfig()
	}
	rs := &replState{vm: runtime.NewState(), cfg: loadREPLConfig(configPath), configPath: configPath, output: os.Stdout}
	if rs.cfg.History == "" {
		rs.cfg.History = filepath.Join(filepath.Dir(configPath), "history")
	}
	rs.vm.XST = xst
	stdlib.RegisterAll(rs.vm)
	rs.vm.Output = rs.output
	editor, err := newREPLEditor(rs.cfg)
	if err != nil {
		fmt.Fprintln(os.Stderr, "initialize REPL:", err)
		return
	}
	rs.editor = editor
	defer rs.editor.Close()
	defer saveREPLConfig(rs.configPath, rs.cfg)

	for {
		line, ok := rs.editor.ReadLine(rs.editor.Prompt())
		if !ok {
			return
		}
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		rs.editor.AddHistory(line)
		if strings.HasPrefix(line, "%") {
			if rs.directive(line) {
				return
			}
			continue
		}
		v, e := exec.EvalChunk(rs.vm, line, "<repl>")
		if e != nil {
			fmt.Fprintln(os.Stderr, e)
		} else {
			writeResult(rs.output, v)
		}
	}
}

func completeREPLDirective(prefix string) []string { return completionMatches(prefix) }

func completionMatches(prefix string) []string {
	var matches []string
	candidates := domscriptKeywords
	if strings.HasPrefix(prefix, "%") {
		candidates = directives
	}
	for _, candidate := range candidates {
		if strings.HasPrefix(candidate, prefix) {
			matches = append(matches, candidate)
		}
	}
	return matches
}

func completionPrefix(input string) string {
	input = strings.TrimLeftFunc(input, unicode.IsSpace)
	if strings.HasPrefix(input, "%") {
		return input
	}
	last := 0
	for i, r := range input {
		if !(r == '_' || r == '%' || r == '.' || unicode.IsLetter(r) || unicode.IsDigit(r)) {
			last = i + len(string(r))
		}
	}
	return input[last:]
}

const (
	replColorRed           = 1
	replColorGreen         = 2
	replColorBrown         = 3
	replColorMagenta       = 5
	replColorCyan          = 6
	replColorGray          = 8
	replColorBrightMagenta = 13
	replColorDefault       = 1 << 16
)

func highlightREPLColors(line string) []int {
	runes := []rune(line)
	colors := make([]int, len(runes))
	for i := range colors {
		colors[i] = replColorDefault
	}
	if len(runes) == 0 {
		return colors
	}

	i := 0
	for i < len(runes) && unicode.IsSpace(runes[i]) {
		i++
	}
	if i < len(runes) && runes[i] == '%' {
		for i < len(runes) && !unicode.IsSpace(runes[i]) {
			colors[i] = replColorCyan
			i++
		}
		return colors
	}

	for i = 0; i < len(runes); {
		r := runes[i]
		if r == '/' && i+1 < len(runes) && runes[i+1] == '/' {
			colorRange(colors, i, len(runes), replColorGray)
			break
		}
		if r == '"' {
			start := i
			i++
			for i < len(runes) {
				if runes[i] == '\\' && i+1 < len(runes) {
					i += 2
					continue
				}
				if runes[i] == '"' {
					i++
					break
				}
				i++
			}
			colorRange(colors, start, i, replColorGreen)
			continue
		}
		if r == ':' {
			start := i
			i++
			for i < len(runes) && (runes[i] == '_' || unicode.IsLetter(runes[i]) || unicode.IsDigit(runes[i])) {
				i++
			}
			colorRange(colors, start, i, replColorBrown)
			continue
		}
		if unicode.IsDigit(r) {
			start := i
			i++
			for i < len(runes) && (unicode.IsDigit(runes[i]) || runes[i] == '.') {
				i++
			}
			colorRange(colors, start, i, replColorBrightMagenta)
			continue
		}
		if r == '_' || unicode.IsLetter(r) {
			start := i
			i++
			for i < len(runes) && (runes[i] == '_' || unicode.IsLetter(runes[i]) || unicode.IsDigit(runes[i])) {
				i++
			}
			if isDomscriptKeyword(string(runes[start:i])) {
				colorRange(colors, start, i, replColorMagenta)
			}
			continue
		}
		if strings.ContainsRune("=<>+-*/%?~.$", r) {
			colors[i] = replColorRed
		}
		i++
	}
	return colors
}

func colorRange(colors []int, start, end, color int) {
	if end > len(colors) {
		end = len(colors)
	}
	for i := start; i < end; i++ {
		colors[i] = color
	}
}

func isDomscriptKeyword(word string) bool {
	for _, keyword := range domscriptKeywords {
		if word == keyword {
			return true
		}
	}
	return false
}

func (rs *replState) directive(line string) bool {
	f := strings.Fields(line)
	if len(f) == 0 {
		return false
	}
	arg := func(n int) string {
		if len(f) > n {
			return strings.Join(f[n:], " ")
		}
		return ""
	}
	switch f[0] {
	case "%quit":
		return true
	case "%load":
		p := arg(1)
		if p == "" {
			fmt.Fprintln(os.Stderr, "usage: %load FILE")
			break
		}
		b, e := os.ReadFile(p)
		if e != nil {
			fmt.Fprintln(os.Stderr, e)
			break
		}
		v, e := exec.EvalChunk(rs.vm, string(b), p)
		if e != nil {
			fmt.Fprintln(os.Stderr, e)
		} else {
			writeResult(rs.output, v)
		}
	case "%library":
		p := arg(1)
		if p == "" {
			fmt.Fprintln(os.Stderr, "usage: %library NAME [DIR]")
			break
		}
		dir := "."
		if len(f) > 2 {
			dir = f[2]
		}
		if _, e := exec.LoadModule(rs.vm, p, dir); e != nil {
			fmt.Fprintln(os.Stderr, e)
		}
	case "%shell":
		command := arg(1)
		if command == "" {
			percent := "%"
			fmt.Fprintln(os.Stderr, "usage: "+percent+"shell COMMAND")
			break
		}
		cmd := shellExec.Command("sh", "-c", command)
		cmd.Stdin, cmd.Stdout, cmd.Stderr = os.Stdin, rs.output, os.Stderr
		if e := cmd.Run(); e != nil {
			fmt.Fprintln(os.Stderr, e)
		}
	case "%theme":
		rs.cfg.Theme = arg(1)
		rs.editor.SetTheme(rs.cfg.Theme)
	case "%jit":
		rs.jit = strings.ToLower(arg(1)) != "off"
	case "%aot":
		rs.aot = strings.ToLower(arg(1)) != "off"
	case "%output":
		p := arg(1)
		if p == "" {
			rs.output = os.Stdout
		} else {
			file, e := os.OpenFile(p, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0600)
			if e != nil {
				fmt.Fprintln(os.Stderr, e)
				break
			}
			rs.output = file
		}
		rs.vm.Output = rs.output
	case "%autocomplete":
		rs.cfg.AutoComplete = strings.ToLower(arg(1)) != "off"
		rs.editor.SetAutocomplete(rs.cfg.AutoComplete)
	case "%highlight":
		rs.cfg.Highlight = strings.ToLower(arg(1)) != "off"
		rs.editor.SetHighlight(rs.cfg.Highlight)
	case "%config":
		if p := arg(1); p != "" {
			rs.configPath = p
			rs.cfg = loadREPLConfig(p)
			rs.editor.SetAutocomplete(rs.cfg.AutoComplete)
			rs.editor.SetHighlight(rs.cfg.Highlight)
			rs.editor.SetTheme(rs.cfg.Theme)
		}
	case "%save":
		p := arg(1)
		if p == "" {
			p = rs.configPath
		}
		if e := saveREPLConfig(p, rs.cfg); e != nil {
			fmt.Fprintln(os.Stderr, e)
		}
	case "%restore":
		p := arg(1)
		if p == "" {
			p = rs.configPath
		}
		rs.cfg = loadREPLConfig(p)
		rs.editor.SetAutocomplete(rs.cfg.AutoComplete)
		rs.editor.SetHighlight(rs.cfg.Highlight)
		rs.editor.SetTheme(rs.cfg.Theme)
	default:
		fmt.Fprintln(os.Stderr, "unknown directive", f[0])
	}
	return false
}
