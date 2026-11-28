package http

import (
	"fmt"
	"io"
	nethttp "net/http"
	"net/url"
	"strings"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__http_"+name, fn)
	}

	reg("get", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("get expects a :String URL")
		}
		resp, err := nethttp.Get(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		defer resp.Body.Close()
		body, _ := io.ReadAll(resp.Body)
		t := runtime.NewTable()
		t.Set(runtime.Str("status"), runtime.Int(int64(resp.StatusCode)))
		t.Set(runtime.Str("body"), runtime.Str(string(body)))
		headerT := runtime.NewTable()
		for k, vals := range resp.Header {
			headerT.Set(runtime.Str(k), runtime.Str(strings.Join(vals, ", ")))
		}
		t.Set(runtime.Str("headers"), runtime.TableValue(headerT))
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("post", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("post expects a :String URL and :String body")
		}
		contentType := "application/json"
		if len(args) > 2 && args[2].Kind() == runtime.KindString {
			contentType = args[2].StrValue()
		}
		resp, err := nethttp.Post(args[0].StrValue(), contentType, strings.NewReader(args[1].StrValue()))
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		defer resp.Body.Close()
		body, _ := io.ReadAll(resp.Body)
		t := runtime.NewTable()
		t.Set(runtime.Str("status"), runtime.Int(int64(resp.StatusCode)))
		t.Set(runtime.Str("body"), runtime.Str(string(body)))
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("request", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("request expects :String method and :String URL")
		}
		method := args[0].StrValue()
		reqURL := args[1].StrValue()
		var body io.Reader
		if len(args) > 2 && args[2].Kind() == runtime.KindString {
			body = strings.NewReader(args[2].StrValue())
		}
		req, err := nethttp.NewRequest(method, reqURL, body)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		if len(args) > 3 && args[3].Kind() == runtime.KindTable {
			args[3].Table().Entries(func(k, v runtime.Value) {
				req.Header.Set(runtime.Display(k), runtime.Display(v))
			})
		}
		resp, err := nethttp.DefaultClient.Do(req)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		defer resp.Body.Close()
		respBody, _ := io.ReadAll(resp.Body)
		t := runtime.NewTable()
		t.Set(runtime.Str("status"), runtime.Int(int64(resp.StatusCode)))
		t.Set(runtime.Str("body"), runtime.Str(string(respBody)))
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("delete", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("delete expects a :String URL")
		}
		req, err := nethttp.NewRequest("DELETE", args[0].StrValue(), nil)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		resp, err := nethttp.DefaultClient.Do(req)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		defer resp.Body.Close()
		body, _ := io.ReadAll(resp.Body)
		t := runtime.NewTable()
		t.Set(runtime.Str("status"), runtime.Int(int64(resp.StatusCode)))
		t.Set(runtime.Str("body"), runtime.Str(string(body)))
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("encode_form", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("encode_form expects a :Table")
		}
		vals := url.Values{}
		args[0].Table().Entries(func(k, v runtime.Value) {
			vals.Set(runtime.Display(k), runtime.Display(v))
		})
		return runtime.Str(vals.Encode()), nil
	})

	reg("decode_form", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("decode_form expects a :String")
		}
		vals, err := url.ParseQuery(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		t := runtime.NewTable()
		for k, vs := range vals {
			t.Set(runtime.Str(k), runtime.Str(strings.Join(vs, ",")))
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})
}
