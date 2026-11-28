// Package testing provides an in-memory transport harness so MeekaMCP servers
// can be exercised without spawning processes or opening network ports.
package testing

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"
	"sync/atomic"

	"hashbrownies.ir/meeka/meekamcp"
)

// TestClient drives a Server through an in-process transport.
type TestClient struct {
	server    *meekamcp.Server
	transport *inProcessTransport
	reqSeq    atomic.Int64
	startOnce sync.Once
	serveErr  chan error
}

// NewTestClient wires an in-process transport into s. The transport is
// started lazily on first call.
func NewTestClient(s *meekamcp.Server) *TestClient {
	return &TestClient{
		server:    s,
		transport: newInProcessTransport(),
		serveErr:  make(chan error, 1),
	}
}

func (c *TestClient) ensureStarted() {
	c.startOnce.Do(func() {
		go func() {
			c.serveErr <- c.server.Serve(context.Background(), c.transport)
		}()
		<-c.transport.ready
	})
}

// Close shuts the in-process transport down.
func (c *TestClient) Close() error { return c.transport.Close() }

// Notifications returns server-initiated frames captured so far (streamed
// tool content, resource-update notifications, ...).
func (c *TestClient) Notifications() []*meekamcp.JSONRPCMessage {
	return c.transport.notifications()
}

func (c *TestClient) call(ctx context.Context, method string, params any) (json.RawMessage, error) {
	c.ensureStarted()
	id := c.reqSeq.Add(1)
	rawID, _ := json.Marshal(id)
	rawParams, err := json.Marshal(params)
	if err != nil {
		return nil, err
	}
	msg := &meekamcp.JSONRPCMessage{
		JSONRPC: meekamcp.JSONRPCVersion,
		ID:      rawID,
		Method:  method,
		Params:  rawParams,
	}
	resp, err := c.transport.roundTrip(ctx, msg)
	if err != nil {
		return nil, err
	}
	if resp.Error != nil {
		return nil, resp.Error
	}
	return resp.Result, nil
}

// Initialize performs the MCP initialize handshake and returns the raw result.
func (c *TestClient) Initialize(ctx context.Context) (json.RawMessage, error) {
	return c.call(ctx, "initialize", map[string]any{
		"protocolVersion": meekamcp.ProtocolVersion,
		"clientInfo":      map[string]any{"name": "meekamcp-testclient", "version": "0"},
	})
}

// CallTool invokes tools/call and decodes the returned content items.
func (c *TestClient) CallTool(ctx context.Context, name string, args any) ([]meekamcp.Content, error) {
	result, err := c.call(ctx, "tools/call", map[string]any{
		"name":      name,
		"arguments": args,
	})
	if err != nil {
		return nil, err
	}
	var wire struct {
		Content []json.RawMessage `json:"content"`
		IsError bool              `json:"isError"`
	}
	if err := json.Unmarshal(result, &wire); err != nil {
		return nil, err
	}
	items, err := decodeContents(wire.Content)
	if err != nil {
		return nil, err
	}
	if wire.IsError {
		text := "tool error"
		if len(items) > 0 {
			if t, ok := items[0].(meekamcp.TextContent); ok {
				text = t.Text
			}
		}
		return items, meekamcp.NewToolError(text)
	}
	return items, nil
}

// ReadResource invokes resources/read and decodes the returned contents.
func (c *TestClient) ReadResource(ctx context.Context, uri string) ([]meekamcp.Content, error) {
	result, err := c.call(ctx, "resources/read", map[string]any{"uri": uri})
	if err != nil {
		return nil, err
	}
	var wire struct {
		Contents []json.RawMessage `json:"contents"`
	}
	if err := json.Unmarshal(result, &wire); err != nil {
		return nil, err
	}
	return decodeContents(wire.Contents)
}

// GetPrompt invokes prompts/get.
func (c *TestClient) GetPrompt(ctx context.Context, name string, args any) (*meekamcp.PromptResult, error) {
	result, err := c.call(ctx, "prompts/get", map[string]any{
		"name":      name,
		"arguments": args,
	})
	if err != nil {
		return nil, err
	}
	var wire struct {
		Description string `json:"description"`
		Messages    []struct {
			Role    meekamcp.Role     `json:"role"`
			Content json.RawMessage `json:"content"`
		} `json:"messages"`
	}
	if err := json.Unmarshal(result, &wire); err != nil {
		return nil, err
	}
	out := &meekamcp.PromptResult{Description: wire.Description}
	for _, m := range wire.Messages {
		content, err := meekamcp.DecodeContent(m.Content)
		if err != nil {
			return nil, err
		}
		out.Messages = append(out.Messages, meekamcp.PromptMessage{Role: m.Role, Content: content})
	}
	return out, nil
}

// ListTools invokes tools/list.
func (c *TestClient) ListTools(ctx context.Context) ([]meekamcp.ToolInfo, error) {
	var all []meekamcp.ToolInfo
	cursor := ""
	for {
		result, err := c.call(ctx, "tools/list", map[string]any{"cursor": cursor})
		if err != nil {
			return nil, err
		}
		var wire struct {
			Tools      []meekamcp.ToolInfo `json:"tools"`
			NextCursor string            `json:"nextCursor"`
		}
		if err := json.Unmarshal(result, &wire); err != nil {
			return nil, err
		}
		all = append(all, wire.Tools...)
		if wire.NextCursor == "" {
			return all, nil
		}
		cursor = wire.NextCursor
	}
}

func decodeContents(raws []json.RawMessage) ([]meekamcp.Content, error) {
	out := make([]meekamcp.Content, 0, len(raws))
	for _, raw := range raws {
		c, err := meekamcp.DecodeContent(raw)
		if err != nil {
			return nil, err
		}
		out = append(out, c)
	}
	return out, nil
}

// inProcessTransport delivers frames synchronously between client and server.
type inProcessTransport struct {
	ready   chan struct{}
	done    chan struct{}
	once    sync.Once
	handler meekamcp.MessageHandler
	mu      sync.Mutex
	notes   []*meekamcp.JSONRPCMessage
}

func newInProcessTransport() *inProcessTransport {
	return &inProcessTransport{ready: make(chan struct{}), done: make(chan struct{})}
}

func (t *inProcessTransport) Start(ctx context.Context, handler meekamcp.MessageHandler) error {
	t.mu.Lock()
	t.handler = handler
	t.mu.Unlock()
	close(t.ready)
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-t.done:
		return nil
	}
}

func (t *inProcessTransport) Send(_ context.Context, msg *meekamcp.JSONRPCMessage) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.notes = append(t.notes, msg)
	return nil
}

func (t *inProcessTransport) Close() error {
	t.once.Do(func() { close(t.done) })
	return nil
}

func (t *inProcessTransport) notifications() []*meekamcp.JSONRPCMessage {
	t.mu.Lock()
	defer t.mu.Unlock()
	return append([]*meekamcp.JSONRPCMessage(nil), t.notes...)
}

func (t *inProcessTransport) roundTrip(ctx context.Context, msg *meekamcp.JSONRPCMessage) (*meekamcp.JSONRPCMessage, error) {
	t.mu.Lock()
	handler := t.handler
	t.mu.Unlock()
	if handler == nil {
		return nil, fmt.Errorf("meekamcp/testing: transport not started")
	}
	resp, err := handler(ctx, msg)
	if err != nil {
		return nil, err
	}
	if resp == nil {
		return nil, fmt.Errorf("meekamcp/testing: no response for request %s", msg.Method)
	}
	return resp, nil
}
