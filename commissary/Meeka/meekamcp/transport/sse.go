package transport

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"sync"

	"hashbrownies.ir/meeka/meekamcp"
)

type sseConfig struct {
	ssePath     string
	messagePath string
}

// SSEOption configures the HTTP+SSE transport.
type SSEOption func(*sseConfig)

// WithSSEPath overrides the SSE upgrade path (default "/sse").
func WithSSEPath(p string) SSEOption {
	return func(c *sseConfig) { c.ssePath = p }
}

// WithMessagePath overrides the message POST path (default "/messages").
func WithMessagePath(p string) SSEOption {
	return func(c *sseConfig) { c.messagePath = p }
}

// sseSession is one connected SSE client.
type sseSession struct {
	id     string
	events chan []byte
	done   chan struct{}
	once   sync.Once
}

func (s *sseSession) close() {
	s.once.Do(func() { close(s.done) })
}

// SSETransport serves MCP over HTTP: GET /sse upgrades to an SSE stream and
// POST /messages?session_id=<id> carries client-to-server JSON-RPC requests.
type SSETransport struct {
	cfg sseConfig

	mu       sync.RWMutex
	sessions map[string]*sseSession
	handler  meekamcp.MessageHandler
	ctx      context.Context
	started  chan struct{}
	closed   chan struct{}
	closeOne sync.Once
}

// NewSSETransport creates an HTTP+SSE transport. Mount Handler() into any
// net/http compatible router and pass the transport to Server.Serve.
func NewSSETransport(opts ...SSEOption) *SSETransport {
	cfg := sseConfig{ssePath: "/sse", messagePath: "/messages"}
	for _, opt := range opts {
		opt(&cfg)
	}
	return &SSETransport{
		cfg:      cfg,
		sessions: map[string]*sseSession{},
		started:  make(chan struct{}),
		closed:   make(chan struct{}),
	}
}

// Start records the message handler and blocks until Close or ctx cancellation.
func (t *SSETransport) Start(ctx context.Context, handler meekamcp.MessageHandler) error {
	t.mu.Lock()
	t.handler = handler
	t.ctx = ctx
	t.mu.Unlock()
	close(t.started)
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-t.closed:
		return nil
	}
}

// Send broadcasts a server-initiated frame (notification) to all sessions.
func (t *SSETransport) Send(ctx context.Context, msg *meekamcp.JSONRPCMessage) error {
	raw, err := json.Marshal(msg)
	if err != nil {
		return err
	}
	t.mu.RLock()
	defer t.mu.RUnlock()
	for _, sess := range t.sessions {
		select {
		case sess.events <- raw:
		case <-sess.done:
		case <-ctx.Done():
			return ctx.Err()
		}
	}
	return nil
}

// Close terminates all sessions and unblocks Start.
func (t *SSETransport) Close() error {
	t.closeOne.Do(func() { close(t.closed) })
	t.mu.Lock()
	defer t.mu.Unlock()
	for id, sess := range t.sessions {
		sess.close()
		delete(t.sessions, id)
	}
	return nil
}

// Handler returns a standard http.Handler that can be mounted into
// net/http, Chi, Gin, etc.
func (t *SSETransport) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc(t.cfg.ssePath, t.handleSSE)
	mux.HandleFunc(t.cfg.messagePath, t.handleMessage)
	return mux
}

func newSessionID() string {
	var buf [16]byte
	if _, err := rand.Read(buf[:]); err != nil {
		return "session-fallback"
	}
	return hex.EncodeToString(buf[:])
}

func (t *SSETransport) handleSSE(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming unsupported", http.StatusInternalServerError)
		return
	}
	sess := &sseSession{
		id:     newSessionID(),
		events: make(chan []byte, 32),
		done:   make(chan struct{}),
	}
	t.mu.Lock()
	t.sessions[sess.id] = sess
	t.mu.Unlock()
	defer func() {
		sess.close()
		t.mu.Lock()
		delete(t.sessions, sess.id)
		t.mu.Unlock()
	}()

	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	// The first event tells the client where to POST messages.
	fmt.Fprintf(w, "event: endpoint\ndata: %s?session_id=%s\n\n", t.cfg.messagePath, sess.id)
	flusher.Flush()

	for {
		select {
		case <-r.Context().Done():
			return
		case <-sess.done:
			return
		case <-t.closed:
			return
		case data := <-sess.events:
			fmt.Fprintf(w, "event: message\ndata: %s\n\n", data)
			flusher.Flush()
		}
	}
}

func (t *SSETransport) handleMessage(w http.ResponseWriter, r *http.Request) {
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
	sessionID := r.URL.Query().Get("session_id")
	t.mu.RLock()
	sess := t.sessions[sessionID]
	handler := t.handler
	t.mu.RUnlock()
	if sess == nil {
		http.Error(w, "unknown session", http.StatusNotFound)
		return
	}
	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "read error", http.StatusBadRequest)
		return
	}
	var msg meekamcp.JSONRPCMessage
	if err := json.Unmarshal(body, &msg); err != nil {
		http.Error(w, "malformed JSON-RPC frame", http.StatusBadRequest)
		return
	}
	resp, err := handler(r.Context(), &msg)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	// Responses flow back over the SSE stream; POST just acknowledges.
	if resp != nil {
		raw, err := json.Marshal(resp)
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		select {
		case sess.events <- raw:
		case <-sess.done:
		case <-r.Context().Done():
		}
	}
	w.WriteHeader(http.StatusAccepted)
}
