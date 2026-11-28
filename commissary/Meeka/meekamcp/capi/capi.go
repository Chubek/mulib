// Package main builds the MeekaMCP C API as a c-shared library.
//
// Build:
//
//	go build -buildmode=c-shared -o libmeekamcp.so ./capi
//
// The curated public header lives in capi/include/meekamcp.h; the cgo-generated
// libmeekamcp.h is a superset used internally.
package main

/*
#include <stdlib.h>

// A tool callback receives the JSON-encoded argument object and the opaque
// user_data pointer given at registration. It must return a malloc(3)
// allocated NUL-terminated string (freed by the library) containing the
// text result, or NULL to signal a tool execution error.
typedef char* (*meekamcp_tool_fn)(const char* args_json, void* user_data);

// A resource callback receives the concrete URI plus the JSON-encoded map of
// URI-template parameters, with the same return convention as tools.
typedef char* (*meekamcp_resource_fn)(const char* uri, const char* params_json, void* user_data);

static char* meekamcp_call_tool_fn(meekamcp_tool_fn fn, const char* args_json, void* user_data) {
	return fn(args_json, user_data);
}

static char* meekamcp_call_resource_fn(meekamcp_resource_fn fn, const char* uri, const char* params_json, void* user_data) {
	return fn(uri, params_json, user_data);
}
*/
import "C"

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"
	"unsafe"

	"hashbrownies.ir/meeka/meekamcp"
	"hashbrownies.ir/meeka/meekamcp/transport"
)

// serverEntry tracks one exported server handle.
type serverEntry struct {
	server  *meekamcp.Server
	cancel  context.CancelFunc
	lastErr string
}

var (
	mu      sync.Mutex
	nextID  C.ulonglong = 1
	servers             = map[C.ulonglong]*serverEntry{}
)

func getEntry(handle C.ulonglong) *serverEntry {
	mu.Lock()
	defer mu.Unlock()
	return servers[handle]
}

func setError(entry *serverEntry, err error) C.int {
	if err == nil {
		entry.lastErr = ""
		return 0
	}
	entry.lastErr = err.Error()
	return -1
}

// rawArgs re-encodes a JSON argument blob into a generic input for handlers.
type rawArgs map[string]any

//export meekamcp_server_new
func meekamcp_server_new(name, version *C.char) C.ulonglong {
	srv := meekamcp.NewServer(meekamcp.ServerInfo{
		Name:    C.GoString(name),
		Version: C.GoString(version),
	})
	mu.Lock()
	defer mu.Unlock()
	handle := nextID
	nextID++
	servers[handle] = &serverEntry{server: srv}
	return handle
}

//export meekamcp_server_free
func meekamcp_server_free(handle C.ulonglong) {
	mu.Lock()
	entry := servers[handle]
	delete(servers, handle)
	mu.Unlock()
	if entry != nil {
		if entry.cancel != nil {
			entry.cancel()
		}
		_ = entry.server.Shutdown(context.Background())
	}
}

//export meekamcp_last_error
func meekamcp_last_error(handle C.ulonglong) *C.char {
	entry := getEntry(handle)
	if entry == nil || entry.lastErr == "" {
		return nil
	}
	return C.CString(entry.lastErr)
}

//export meekamcp_string_free
func meekamcp_string_free(s *C.char) {
	C.free(unsafe.Pointer(s))
}

// invokeToolCallback marshals args and bridges into the C function pointer.
func invokeToolCallback(fn C.meekamcp_tool_fn, userData unsafe.Pointer, args rawArgs) (string, error) {
	blob, err := json.Marshal(args)
	if err != nil {
		return "", err
	}
	cArgs := C.CString(string(blob))
	defer C.free(unsafe.Pointer(cArgs))
	out := C.meekamcp_call_tool_fn(fn, cArgs, userData)
	if out == nil {
		return "", fmt.Errorf("tool callback reported an error")
	}
	defer C.free(unsafe.Pointer(out))
	return C.GoString(out), nil
}

//export meekamcp_add_tool
func meekamcp_add_tool(handle C.ulonglong, name, description, schemaJSON *C.char, fn C.meekamcp_tool_fn, userData unsafe.Pointer) C.int {
	entry := getEntry(handle)
	if entry == nil {
		return -1
	}
	toolName := C.GoString(name)
	opts := []meekamcp.ToolOption{meekamcp.WithToolDescription(C.GoString(description))}
	if schemaJSON != nil {
		opts = append(opts, meekamcp.WithSchemaOverride(json.RawMessage(C.GoString(schemaJSON))))
	} else {
		// Free-form object schema when the C side does not provide one.
		opts = append(opts, meekamcp.WithSchemaOverride(json.RawMessage(`{"type":"object","additionalProperties":true}`)))
	}
	err := meekamcp.AddTool(entry.server, toolName, func(_ context.Context, in rawArgs) (meekamcp.TextContent, error) {
		text, err := invokeToolCallback(fn, userData, in)
		if err != nil {
			return meekamcp.TextContent{}, err
		}
		return meekamcp.TextContent{Text: text}, nil
	}, opts...)
	return setError(entry, err)
}

//export meekamcp_add_resource
func meekamcp_add_resource(handle C.ulonglong, uri, name, description, mimeType *C.char, fn C.meekamcp_resource_fn, userData unsafe.Pointer) C.int {
	entry := getEntry(handle)
	if entry == nil {
		return -1
	}
	var opts []meekamcp.ResourceOption
	if name != nil {
		opts = append(opts, meekamcp.WithResourceName(C.GoString(name)))
	}
	if description != nil {
		opts = append(opts, meekamcp.WithResourceDescription(C.GoString(description)))
	}
	if mimeType != nil {
		opts = append(opts, meekamcp.WithResourceMimeType(C.GoString(mimeType)))
	}
	err := meekamcp.AddResource(entry.server, C.GoString(uri), func(_ context.Context, req meekamcp.ResourceReadRequest) ([]meekamcp.Content, error) {
		params, err := json.Marshal(req.Params)
		if err != nil {
			return nil, err
		}
		cURI := C.CString(req.URI)
		cParams := C.CString(string(params))
		defer C.free(unsafe.Pointer(cURI))
		defer C.free(unsafe.Pointer(cParams))
		out := C.meekamcp_call_resource_fn(fn, cURI, cParams, userData)
		if out == nil {
			return nil, meekamcp.NewResourceNotFoundError(req.URI)
		}
		defer C.free(unsafe.Pointer(out))
		return []meekamcp.Content{meekamcp.TextContent{Text: C.GoString(out)}}, nil
	}, opts...)
	return setError(entry, err)
}

//export meekamcp_handle_message
func meekamcp_handle_message(handle C.ulonglong, requestJSON *C.char) *C.char {
	entry := getEntry(handle)
	if entry == nil {
		return nil
	}
	resp, err := entry.server.HandleMessage(context.Background(), []byte(C.GoString(requestJSON)))
	if err != nil {
		setError(entry, err)
		return nil
	}
	if resp == nil {
		// Notification: no response frame.
		setError(entry, nil)
		return nil
	}
	return C.CString(string(resp))
}

//export meekamcp_serve_stdio
func meekamcp_serve_stdio(handle C.ulonglong) C.int {
	entry := getEntry(handle)
	if entry == nil {
		return -1
	}
	ctx, cancel := context.WithCancel(context.Background())
	mu.Lock()
	entry.cancel = cancel
	mu.Unlock()
	err := entry.server.Serve(ctx, transport.NewStdioTransport(transport.WithStdoutGuard()))
	return setError(entry, err)
}

//export meekamcp_shutdown
func meekamcp_shutdown(handle C.ulonglong) C.int {
	entry := getEntry(handle)
	if entry == nil {
		return -1
	}
	if entry.cancel != nil {
		entry.cancel()
	}
	return setError(entry, entry.server.Shutdown(context.Background()))
}

func main() {}
