package domscript

import (
	"fmt"
	"testing"
)

func TestRegisterNativeUsesPublicValueAPI(t *testing.T) {
	st := Open()
	defer Close(st)

	RegisterNative(st, "host_slug", func(st *State, args []Value) (Value, error) {
		if len(args) != 1 || args[0].Kind() != KindString {
			return Unit(), fmt.Errorf("host_slug expects one string")
		}
		if args[0].Str() == "" {
			return Tagged("Error", Str("empty title")), nil
		}
		return Str("slug-" + args[0].Str()), nil
	})

	got, err := Eval(st, `host_slug("Guide")`, "native_test.doml")
	if err != nil {
		t.Fatal(err)
	}
	if got.Kind() != KindString || got.Str() != "slug-Guide" {
		t.Fatalf("got %v %q", got.Kind(), got.Str())
	}
}
