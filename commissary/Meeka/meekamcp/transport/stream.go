package transport

import (
	"bufio"
	"context"
	"encoding/json"
	"net/http"
	"sync"

	"hashbrownies.ir/meeka/meekamcp"
)

type streamableConfig struct {
	path string
}

// StreamableHTTPOption configures the streamable HTTP transport.
type StreamableHTTPOption func(*streamableConfig)

// WithStreamPath overrides the single POST endpoint path (default "/mcp").
func WithStreamPath(p string) StreamableHTTPOption {
	return func(c *streamableConfig) { c.path = p }
}

// StreamableHTTPTransport implements the single-endpoint HTTP POST streaming
// variant: the client POSTs newline-delimited JSON-RPC frames and receives a
// continuous JSON-line response stream on the same connection.
type StreamableHTTPTransport struct {
	cfg streamableConfig

	mu       sync.RWMutex
	handler  meekamcp.MessageHandler
	started  chan struct{}
	closed   chan struct{}
	closeOne sync.Once

	// broadcast targets for server-initiated frames
	streamsMu sync.Mutex
	streams   map[chan []byte]struct{}
}

// NewStreamableHTTPTransport creates a streamable HTTP transport.
func NewStreamableHTTPTransport(opts ...StreamableHTTPOption) *StreamableHTTPTransport {
	cfg := streamableConfig{path: "/mcp"}
	for _, opt := range opts {
		opt(&cfg)
	}
	return &StreamableHTTPTransport{
		cfg:     cfg,
		started: make(chan struct{}),
		closed:  make(chan struct{}),
		streams: map[chan []byte]struct{}{},
	}
}

// Start records the handler and blocks until Close or ctx cancellation.
func (t *StreamableHTTPTransport) Start(ctx context.Context, handler meekamcp.MessageHandler) error {
	t.mu.Lock()
	t.handler = handler
	t.mu.Unlock()
	close(t.started)
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-t.closed:
		return nil
	}
}

// Send broadcasts a server-initiated frame to all open streams.
func (t *StreamableHTTPTransport) Send(ctx context.Context, msg *meekamcp.JSONRPCMessage) error {
	raw, err := json.Marshal(msg)
	if err != nil {
		return err
	}
	t.streamsMu.Lock()
	defer t.streamsMu.Unlock()
	for ch := range t.streams {
		select {
		case ch <- raw:
		case <-ctx.Done():
			return ctx.Err()
		default:
			// Slow consumer; drop rather than block the server.
		}
	}
	return nil
}

// Close unblocks Start and terminates open request streams.
func (t *StreamableHTTPTransport) Close() error {
	t.closeOne.Do(func() { close(t.closed) })
	return nil
}

// Handler returns the single-endpoint http.Handler.
func (t *StreamableHTTPTransport) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc(t.cfg.path, t.handlePost)
	return mux
}

func (t *StreamableHTTPTransport) handlePost(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	select {
	case <-t.started:
	default:
		http.Error(w, "transport not started", http.StatusServiceUnavailable)
		return
	}
	t.mu.RLock()
	handler := t.handler
	t.mu.RUnlock()

	flusher, canFlush := w.(http.Flusher)
	w.Header().Set("Content-Type", "application/x-ndjson")

	outbound := make(chan []byte, 32)
	t.streamsMu.Lock()
	t.streams[outbound] = struct{}{}
	t.streamsMu.Unlock()
	defer func() {
		t.streamsMu.Lock()
		delete(t.streams, outbound)
		t.streamsMu.Unlock()
	}()

	var writeMu sync.Mutex
	writeFrame := func(raw []byte) {
		writeMu.Lock()
		defer writeMu.Unlock()
		w.Write(raw)
		w.Write([]byte{'\n'})
		if canFlush {
			flusher.Flush()
		}
	}

	// Pump server-initiated frames (streamed tool content, notifications)
	// concurrently with request processing.
	pumpDone := make(chan struct{})
	go func() {
		defer close(pumpDone)
		for {
			select {
			case raw := <-outbound:
				writeFrame(raw)
			case <-r.Context().Done():
				return
			case <-t.closed:
				return
			}
		}
	}()

	scanner := bufio.NewScanner(r.Body)
	scanner.Buffer(make([]byte, 0, 64*1024), 16*1024*1024)
	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}
		var msg meekamcp.JSONRPCMessage
		if err := json.Unmarshal(line, &msg); err != nil {
			raw, _ := json.Marshal(&meekamcp.JSONRPCMessage{
				JSONRPC: meekamcp.JSONRPCVersion,
				Error:   &meekamcp.MCPError{Code: meekamcp.CodeParseError, Message: err.Error()},
			})
			writeFrame(raw)
			continue
		}
		resp, err := handler(r.Context(), &msg)
		if err != nil {
			raw, _ := json.Marshal(&meekamcp.JSONRPCMessage{
				JSONRPC: meekamcp.JSONRPCVersion,
				ID:      msg.ID,
				Error:   &meekamcp.MCPError{Code: meekamcp.CodeInternalError, Message: err.Error()},
			})
			writeFrame(raw)
			continue
		}
		if resp != nil {
			raw, err := json.Marshal(resp)
			if err == nil {
				writeFrame(raw)
			}
		}
	}
}
