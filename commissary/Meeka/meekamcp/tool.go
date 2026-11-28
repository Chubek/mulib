package meekamcp

import (
	"context"
	"encoding/json"
	"fmt"
	"reflect"
)

// ToolHandler is the function signature for typed tools.
type ToolHandler[I any, O Content] func(ctx context.Context, input I) (O, error)

// StreamingToolHandler is the signature for tools that stream partial output.
type StreamingToolHandler[I any, O Content] func(
	ctx context.Context,
	input I,
	stream ToolStream[O],
) error

// ToolStream lets a handler push incremental Content before the final return.
type ToolStream[O Content] interface {
	// Write sends a partial content item to the client immediately.
	// Returns an error if the client has disconnected or ctx is done.
	Write(ctx context.Context, item O) error
}

type toolConfig struct {
	description    string
	annotations    map[string]string
	middleware     []Middleware
	schemaOverride json.RawMessage
}

// ToolOption customises tool registration.
type ToolOption func(*toolConfig)

// WithToolDescription overrides the human-readable description.
func WithToolDescription(desc string) ToolOption {
	return func(c *toolConfig) { c.description = desc }
}

// WithToolAnnotations attaches arbitrary string annotations to the tool
// metadata (title, readOnlyHint, destructiveHint, idempotentHint, etc.).
func WithToolAnnotations(a map[string]string) ToolOption {
	return func(c *toolConfig) { c.annotations = a }
}

// WithToolMiddleware adds tool-scoped middleware that only runs for this tool.
func WithToolMiddleware(m ...Middleware) ToolOption {
	return func(c *toolConfig) { c.middleware = append(c.middleware, m...) }
}

// WithSchemaOverride replaces the auto-generated schema entirely.
func WithSchemaOverride(schema json.RawMessage) ToolOption {
	return func(c *toolConfig) { c.schemaOverride = schema }
}

// ToolInfo is the wire representation advertised by tools/list.
type ToolInfo struct {
	Name        string            `json:"name"`
	Description string            `json:"description,omitempty"`
	InputSchema json.RawMessage   `json:"inputSchema"`
	Annotations map[string]string `json:"annotations,omitempty"`
}

// rawToolHandler is the type-erased execution entry point. The stream sink
// may be nil for non-streaming clients/transports.
type rawToolHandler func(ctx context.Context, args json.RawMessage, sink func(Content) error) ([]Content, error)

type registeredTool struct {
	info       ToolInfo
	handler    rawToolHandler
	middleware []Middleware
	streaming  bool
}

// AddTool registers a typed tool on s.
//
// I must be a struct or pointer-to-struct; MeekaMCP derives the JSON Schema
// from exported fields and their `json`, `jsonschema`, and `doc` struct tags.
func AddTool[I any, O Content](
	s *Server,
	name string,
	handler ToolHandler[I, O],
	opts ...ToolOption,
) error {
	if handler == nil {
		return fmt.Errorf("meekamcp: tool %q: nil handler", name)
	}
	raw := func(ctx context.Context, args json.RawMessage, _ func(Content) error) ([]Content, error) {
		input, err := decodeInput[I](args)
		if err != nil {
			return nil, err
		}
		out, err := handler(ctx, input)
		if err != nil {
			return nil, err
		}
		return []Content{out}, nil
	}
	return registerTool[I](s, name, raw, false, opts)
}

// AddStreamingTool is the streaming variant of AddTool.
func AddStreamingTool[I any, O Content](
	s *Server,
	name string,
	handler StreamingToolHandler[I, O],
	opts ...ToolOption,
) error {
	if handler == nil {
		return fmt.Errorf("meekamcp: tool %q: nil handler", name)
	}
	raw := func(ctx context.Context, args json.RawMessage, sink func(Content) error) ([]Content, error) {
		input, err := decodeInput[I](args)
		if err != nil {
			return nil, err
		}
		collector := &streamCollector[O]{sink: sink}
		if err := handler(ctx, input, collector); err != nil {
			return nil, err
		}
		return collector.items, nil
	}
	return registerTool[I](s, name, raw, true, opts)
}

// streamCollector forwards items to the transport sink (when available) and
// accumulates them so the final result carries the full content list.
type streamCollector[O Content] struct {
	sink  func(Content) error
	items []Content
}

func (c *streamCollector[O]) Write(ctx context.Context, item O) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	c.items = append(c.items, item)
	if c.sink != nil {
		return c.sink(item)
	}
	return nil
}

func decodeInput[I any](args json.RawMessage) (I, error) {
	var input I
	if len(args) == 0 {
		return input, nil
	}
	if err := json.Unmarshal(args, &input); err != nil {
		var zero I
		return zero, NewInvalidParamsError("invalid tool arguments", err.Error())
	}
	return input, nil
}

func registerTool[I any](s *Server, name string, raw rawToolHandler, streaming bool, opts []ToolOption) error {
	if s == nil {
		return fmt.Errorf("meekamcp: nil server")
	}
	if name == "" {
		return fmt.Errorf("meekamcp: tool name is required")
	}
	cfg := toolConfig{}
	for _, opt := range opts {
		opt(&cfg)
	}
	var schema json.RawMessage
	if cfg.schemaOverride != nil {
		schema = cfg.schemaOverride
	} else {
		m, err := SchemaFor(reflect.TypeFor[I]())
		if err != nil {
			return fmt.Errorf("meekamcp: tool %q: %w", name, err)
		}
		schema, err = json.Marshal(m)
		if err != nil {
			return fmt.Errorf("meekamcp: tool %q: %w", name, err)
		}
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.tools[name]; exists {
		return fmt.Errorf("meekamcp: duplicate tool %q", name)
	}
	s.tools[name] = &registeredTool{
		info: ToolInfo{
			Name:        name,
			Description: cfg.description,
			InputSchema: schema,
			Annotations: cfg.annotations,
		},
		handler:    raw,
		middleware: cfg.middleware,
		streaming:  streaming,
	}
	return nil
}

// --- server-side tool endpoints ---

type listToolsResult struct {
	Tools      []ToolInfo `json:"tools"`
	NextCursor string     `json:"nextCursor,omitempty"`
}

func (s *Server) listTools(cursor string) (any, error) {
	s.mu.RLock()
	names := s.sortedToolNames()
	infos := make([]ToolInfo, 0, len(names))
	for _, n := range names {
		infos = append(infos, s.tools[n].info)
	}
	s.mu.RUnlock()
	start, end, next, err := s.paginate(len(infos), cursor)
	if err != nil {
		return nil, err
	}
	return listToolsResult{Tools: infos[start:end], NextCursor: next}, nil
}

type callToolWireResult struct {
	Content []json.RawMessage `json:"content"`
	IsError bool              `json:"isError,omitempty"`
}

func (s *Server) callTool(ctx context.Context, info RequestInfo, name string, args json.RawMessage, t Transport, reqID json.RawMessage) (any, error) {
	s.mu.RLock()
	tool, ok := s.tools[name]
	s.mu.RUnlock()
	if !ok {
		return nil, NewMethodNotFoundError("tool " + name)
	}
	// Stream partial content as notifications when the transport supports it.
	var sink func(Content) error
	if tool.streaming && t != nil {
		sink = func(item Content) error {
			raw, err := marshalContent(item)
			if err != nil {
				return err
			}
			note, err := newNotification("notifications/progress", map[string]any{
				"requestId": reqID,
				"content":   json.RawMessage(raw),
			})
			if err != nil {
				return err
			}
			return t.Send(ctx, note)
		}
	}
	terminal := func(ctx context.Context, _ RequestInfo, payload any) (any, error) {
		raw, _ := payload.(json.RawMessage)
		return tool.handler(ctx, raw, sink)
	}
	out, err := s.runChain(ctx, info, args, tool.middleware, terminal)
	if err != nil {
		// Tool failures become isError results so an LLM can observe them.
		me := asMCPError(err, CodeToolExecution)
		if me.Code == CodeToolExecution {
			raw, merr := marshalContent(TextContent{Text: me.Message})
			if merr != nil {
				return nil, me
			}
			return callToolWireResult{Content: []json.RawMessage{raw}, IsError: true}, nil
		}
		return nil, me
	}
	items, _ := out.([]Content)
	rawItems, err := marshalContents(items)
	if err != nil {
		return nil, err
	}
	if rawItems == nil {
		rawItems = []json.RawMessage{}
	}
	return callToolWireResult{Content: rawItems}, nil
}
