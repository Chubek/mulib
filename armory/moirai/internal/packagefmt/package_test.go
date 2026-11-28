package packagefmt

import (
	"os"
	"path/filepath"
	"testing"
)

func TestRoundTrip(t *testing.T) {
	b, err := Build([]byte("1 + 2"), false, true, []string{"math"}, map[string][]byte{"math.doml": []byte("let x = 1")})
	if err != nil {
		t.Fatal(err)
	}
	p := filepath.Join(t.TempDir(), "x.dsca")
	if err := os.WriteFile(p, b, 0600); err != nil {
		t.Fatal(err)
	}
	got, err := Read(p)
	if err != nil {
		t.Fatal(err)
	}
	if string(got.Header.Magic[:]) != "DOM1" || string(got.Code) != "1 + 2" || len(got.Deps) != 1 || len(got.Files) != 1 {
		t.Fatalf("bad package: %#v", got)
	}
}

func TestNativeMagic(t *testing.T) {
	b, err := Build([]byte("code"), true, false, nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	p := filepath.Join(t.TempDir(), "x.dsca")
	if err := os.WriteFile(p, b, 0600); err != nil {
		t.Fatal(err)
	}
	got, err := Read(p)
	if err != nil {
		t.Fatal(err)
	}
	if string(got.Header.Magic[:]) != "DOM2" {
		t.Fatalf("magic = %q", got.Header.Magic)
	}
}
