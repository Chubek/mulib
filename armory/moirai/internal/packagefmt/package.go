package packagefmt

import (
	"archive/zip"
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

const Version uint16 = 1

type Header struct {
	Magic      [4]byte
	Version    uint16
	Flags      uint16
	Deps       uint32
	CodeOffset uint64
	CodeSize   uint64
}
type Package struct {
	Header Header
	Code   []byte
	Deps   []string
	Files  map[string][]byte
}

func Build(code []byte, native, static bool, deps []string, files map[string][]byte) ([]byte, error) {
	var h Header
	if native {
		copy(h.Magic[:], "DOM2")
	} else {
		copy(h.Magic[:], "DOM1")
	}
	h.Version = Version
	if static {
		h.Flags = 1
	}
	h.Deps = uint32(len(deps))
	h.CodeOffset = uint64(binary.Size(h))
	h.CodeSize = uint64(len(code))
	var raw bytes.Buffer
	if err := binary.Write(&raw, binary.LittleEndian, &h); err != nil {
		return nil, err
	}
	raw.Write(code)
	var out bytes.Buffer
	zw := zip.NewWriter(&out)
	w, err := zw.Create("main.bin")
	if err != nil {
		return nil, err
	}
	if _, err = w.Write(raw.Bytes()); err != nil {
		return nil, err
	}
	if len(deps) > 0 {
		d, err := zw.Create("deps.txt")
		if err != nil {
			return nil, err
		}
		for _, x := range deps {
			if _, err = fmt.Fprintln(d, x); err != nil {
				return nil, err
			}
		}
	}
	for n, b := range files {
		if n == "" || filepath.IsAbs(n) || strings.Contains(filepath.ToSlash(n), "../") {
			return nil, fmt.Errorf("invalid library path %q", n)
		}
		w, err := zw.Create("lib/" + filepath.ToSlash(n))
		if err != nil {
			return nil, err
		}
		if _, err = w.Write(b); err != nil {
			return nil, err
		}
	}
	if err := zw.Close(); err != nil {
		return nil, err
	}
	return out.Bytes(), nil
}
func Read(path string) (*Package, error) {
	b, e := os.ReadFile(path)
	if e != nil {
		return nil, e
	}
	zr, e := zip.NewReader(bytes.NewReader(b), int64(len(b)))
	if e != nil {
		return nil, e
	}
	var p Package
	p.Files = map[string][]byte{}
	for _, f := range zr.File {
		r, e := f.Open()
		if e != nil {
			return nil, e
		}
		d, _ := io.ReadAll(r)
		r.Close()
		if f.Name == "main.bin" {
			if len(d) < binary.Size(Header{}) {
				return nil, fmt.Errorf("invalid dsca")
			}
			if err := binary.Read(bytes.NewReader(d), binary.LittleEndian, &p.Header); err != nil {
				return nil, fmt.Errorf("invalid dsca header: %w", err)
			}
			if p.Header.Version != Version {
				return nil, fmt.Errorf("unsupported dsca version %d", p.Header.Version)
			}
			if string(p.Header.Magic[:]) != "DOM1" && string(p.Header.Magic[:]) != "DOM2" {
				return nil, fmt.Errorf("invalid Domscript magic")
			}
			if p.Header.CodeOffset+p.Header.CodeSize > uint64(len(d)) {
				return nil, fmt.Errorf("invalid code offsets")
			}
			p.Code = d[p.Header.CodeOffset : p.Header.CodeOffset+p.Header.CodeSize]
		} else if f.Name == "deps.txt" {
			for _, x := range bytes.Split(bytes.TrimSpace(d), []byte("\n")) {
				if len(x) > 0 {
					p.Deps = append(p.Deps, string(x))
				}
			}
		} else if len(f.Name) > 4 && f.Name[:4] == "lib/" {
			p.Files[f.Name[4:]] = d
		}
	}
	if p.Code == nil {
		return nil, fmt.Errorf("archive missing main.bin")
	}
	return &p, nil
}
