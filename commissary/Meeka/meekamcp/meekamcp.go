// Package meekamcp is a Go library for building Model Context Protocol (MCP)
// servers. It provides typed tool/resource/prompt registration with automatic
// JSON Schema generation, a middleware pipeline, structured MCP errors, and
// pluggable transports (stdio, HTTP+SSE, streamable HTTP).
//
// See .agents/MEEKAMCP.md for the full specification.
package meekamcp

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"sort"
	"strconv"
	"sync"
	"sync/atomic"
)

// ServerInfo is reported to clients during the initialize handshake.
type ServerInfo struct {
	Name    string
	Version string
}

// Capabilities advertises the optional MCP capability sets.
type Capabilities struct {
	Tools                 bool
	Resources             bool
	ResourceSubscriptions bool
	Prompts               bool
}

type serverConfig struct {
	maxConcurrent   int
	logger          *slog.Logger
	capabilities    *Capabilities
	paginationLimit int
}

// ServerOption is a functional option for NewServer.
type ServerOption func(*serverConfig)

// WithMaxConcurrentRequests limits parallel in-flight tool calls per session.
func WithMaxConcurrentRequests(n int) ServerOption {
	return func(c *serverConfig) { c.maxConcurrent = n }
}

// WithLogger attaches a structured logger.
func WithLogger(l *slog.Logger) ServerOption {
	return func(c *serverConfig) { c.logger = l }
}

// WithCapabilities explicitly advertises optional MCP capability sets.
// By default, capabilities are inferred from what has been registered.
func WithCapabilities(caps Capabilities) ServerOption {
	return func(c *serverConfig) { c.capabilities = &caps }
}

// WithPaginationLimit sets the default page size for list* methods.
func WithPaginationLimit(n int) ServerOption {
	return func(c *serverConfig) { c.paginationLimit = n }
}

// Server hosts tools, resources, and prompts and dispatches incoming
// JSON-RPC requests from any attached transport. It is safe for concurrent use.
type Server struct {
	info ServerInfo
	cfg  serverConfig

	mu         sync.RWMutex
	tools      map[string]*registeredTool
	resources  []*registeredResource
	prompts    map[string]*registeredPrompt
	middleware []Middleware

	sessionSeq atomic.Int64
	inflight   sync.WaitGroup
	closed     atomic.Bool

	transportsMu sync.Mutex
	transports   map[Transport]struct{}
}

// NewServer creates a Server. Options are applied in order.
func NewServer(info ServerInfo, opts ...ServerOption) *Server {
	cfg := serverConfig{
		maxConcurrent:   0,
		logger:          slog.New(slog.DiscardHandler),
		paginationLimit: 0,
	}
	for _, opt := range opts {
		opt(&cfg)
	}
	return &Server{
		info:       info,
		cfg:        cfg,
		tools:      map[string]*registeredTool{},
		prompts:    map[string]*registeredPrompt{},
		transports: map[Transport]struct{}{},
	}
}

// UseMiddleware appends middleware to the interceptor chain. Middleware
// is applied in registration order, outermost first.
func (s *Server) UseMiddleware(m ...Middleware) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.middleware = append(s.middleware, m...)
}

// Serve attaches a transport and blocks until the transport is closed
// or ctx is cancelled.
func (s *Server) Serve(ctx context.Context, t Transport) error {
	if s.closed.Load() {
		return errors.New("meekamcp: server is shut down")
	}
	s.transportsMu.Lock()
	s.transports[t] = struct{}{}
	s.transportsMu.Unlock()
	defer func() {
		s.transportsMu.Lock()
		delete(s.transports, t)
		s.transportsMu.Unlock()
	}()

	sessionID := fmt.Sprintf("session-%d", s.sessionSeq.Add(1))
	var sem chan struct{}
	if s.cfg.maxConcurrent > 0 {
		sem = make(chan struct{}, s.cfg.maxConcurrent)
	}
	handler := func(ctx context.Context, msg *JSONRPCMessage) (*JSONRPCMessage, error) {
		if sem != nil {
			select {
			case sem <- struct{}{}:
				defer func() { <-sem }()
			case <-ctx.Done():
				return nil, ctx.Err()
			}
		}
		s.inflight.Add(1)
		defer s.inflight.Done()
		return s.dispatch(ctx, sessionID, msg, t), nil
	}
	return t.Start(ctx, handler)
}

// Shutdown gracefully drains in-flight requests, then closes all transports.
func (s *Server) Shutdown(ctx context.Context) error {
	s.closed.Store(true)
	done := make(chan struct{})
	go func() {
		s.inflight.Wait()
		close(done)
	}()
	select {
	case <-done:
	case <-ctx.Done():
		return ctx.Err()
	}
	s.transportsMu.Lock()
	defer s.transportsMu.Unlock()
	var firstErr error
	for t := range s.transports {
		if err := t.Close(); err != nil && firstErr == nil {
			firstErr = err
		}
		delete(s.transports, t)
	}
	return firstErr
}

func (s *Server) logger() *slog.Logger { return s.cfg.logger }

// HandleMessage processes one raw JSON-RPC frame without a transport. It is
// intended for embedders (for example the C API) that manage their own I/O.
// The returned slice is nil for notifications.
func (s *Server) HandleMessage(ctx context.Context, raw []byte) ([]byte, error) {
	var msg JSONRPCMessage
	if err := json.Unmarshal(raw, &msg); err != nil {
		resp := newErrorResponse(nil, &MCPError{Code: CodeParseError, Message: err.Error()})
		return json.Marshal(resp)
	}
	s.inflight.Add(1)
	defer s.inflight.Done()
	resp := s.dispatch(ctx, "embedded", &msg, nil)
	if resp == nil {
		return nil, nil
	}
	return json.Marshal(resp)
}

// --- Dispatcher ---

// dispatch routes one inbound frame. It never returns a nil response for a
// request frame; notifications yield nil.
func (s *Server) dispatch(ctx context.Context, sessionID string, msg *JSONRPCMessage, t Transport) *JSONRPCMessage {
	if msg == nil || msg.Method == "" {
		return newErrorResponse(nil, &MCPError{Code: CodeInvalidRequest, Message: "invalid request"})
	}
	if msg.IsNotification() {
		// notifications/initialized and cancellations are accepted silently.
		return nil
	}
	result, err := s.route(ctx, sessionID, msg, t)
	if err != nil {
		return newErrorResponse(msg.ID, asMCPError(err, CodeInternalError))
	}
	resp, merr := newResponse(msg.ID, result)
	if merr != nil {
		return newErrorResponse(msg.ID, &MCPError{Code: CodeInternalError, Message: merr.Error()})
	}
	return resp
}

func (s *Server) route(ctx context.Context, sessionID string, msg *JSONRPCMessage, t Transport) (any, error) {
	info := RequestInfo{
		Method:    msg.Method,
		SessionID: sessionID,
		Metadata:  map[string]any{},
	}
	switch msg.Method {
	case "initialize":
		return s.handleInitialize(), nil
	case "ping":
		return map[string]any{}, nil
	case "tools/list":
		var p listParams
		_ = json.Unmarshal(msg.Params, &p)
		return s.listTools(p.Cursor)
	case "tools/call":
		var p struct {
			Name      string          `json:"name"`
			Arguments json.RawMessage `json:"arguments"`
		}
		if err := json.Unmarshal(msg.Params, &p); err != nil {
			return nil, NewInvalidParamsError("malformed tools/call params", err.Error())
		}
		info.Target = p.Name
		return s.callTool(ctx, info, p.Name, p.Arguments, t, msg.ID)
	case "resources/list":
		var p listParams
		_ = json.Unmarshal(msg.Params, &p)
		return s.listResources(p.Cursor)
	case "resources/templates/list":
		var p listParams
		_ = json.Unmarshal(msg.Params, &p)
		return s.listResourceTemplates(p.Cursor)
	case "resources/read":
		var p struct {
			URI string `json:"uri"`
		}
		if err := json.Unmarshal(msg.Params, &p); err != nil {
			return nil, NewInvalidParamsError("malformed resources/read params", err.Error())
		}
		info.Target = p.URI
		return s.readResource(ctx, info, p.URI)
	case "resources/subscribe":
		var p struct {
			URI string `json:"uri"`
		}
		if err := json.Unmarshal(msg.Params, &p); err != nil {
			return nil, NewInvalidParamsError("malformed resources/subscribe params", err.Error())
		}
		return s.subscribeResource(ctx, p.URI, t)
	case "prompts/list":
		var p listParams
		_ = json.Unmarshal(msg.Params, &p)
		return s.listPrompts(p.Cursor)
	case "prompts/get":
		var p struct {
			Name      string          `json:"name"`
			Arguments json.RawMessage `json:"arguments"`
		}
		if err := json.Unmarshal(msg.Params, &p); err != nil {
			return nil, NewInvalidParamsError("malformed prompts/get params", err.Error())
		}
		info.Target = p.Name
		return s.getPrompt(ctx, info, p.Name, p.Arguments)
	default:
		return nil, NewMethodNotFoundError(msg.Method)
	}
}

type listParams struct {
	Cursor string `json:"cursor,omitempty"`
}

func (s *Server) handleInitialize() any {
	s.mu.RLock()
	caps := s.cfg.capabilities
	inferred := Capabilities{
		Tools:     len(s.tools) > 0,
		Resources: len(s.resources) > 0,
		Prompts:   len(s.prompts) > 0,
	}
	for _, r := range s.resources {
		if r.subscribable {
			inferred.ResourceSubscriptions = true
		}
	}
	s.mu.RUnlock()
	if caps == nil {
		caps = &inferred
	}
	capObj := map[string]any{}
	if caps.Tools {
		capObj["tools"] = map[string]any{"listChanged": false}
	}
	if caps.Resources {
		res := map[string]any{"listChanged": false}
		if caps.ResourceSubscriptions {
			res["subscribe"] = true
		}
		capObj["resources"] = res
	}
	if caps.Prompts {
		capObj["prompts"] = map[string]any{"listChanged": false}
	}
	return map[string]any{
		"protocolVersion": ProtocolVersion,
		"capabilities":    capObj,
		"serverInfo": map[string]any{
			"name":    s.info.Name,
			"version": s.info.Version,
		},
	}
}

// paginate slices names for list* endpoints using opaque decimal cursors.
func (s *Server) paginate(total int, cursor string) (start, end int, next string, err error) {
	if cursor != "" {
		start, err = strconv.Atoi(cursor)
		if err != nil || start < 0 || start > total {
			return 0, 0, "", NewInvalidParamsError(fmt.Sprintf("invalid cursor %q", cursor), nil)
		}
	}
	end = total
	if s.cfg.paginationLimit > 0 && start+s.cfg.paginationLimit < total {
		end = start + s.cfg.paginationLimit
		next = strconv.Itoa(end)
	}
	return start, end, next, nil
}

// runChain executes the global middleware chain around a terminal handler.
func (s *Server) runChain(ctx context.Context, info RequestInfo, payload any, extra []Middleware, terminal HandlerFunc) (any, error) {
	s.mu.RLock()
	global := append([]Middleware(nil), s.middleware...)
	s.mu.RUnlock()
	chain := append(global, extra...)
	return chainMiddleware(chain, terminal)(ctx, info, payload)
}

// sortedToolNames returns registered tool names in stable order.
func (s *Server) sortedToolNames() []string {
	names := make([]string, 0, len(s.tools))
	for name := range s.tools {
		names = append(names, name)
	}
	sort.Strings(names)
	return names
}
