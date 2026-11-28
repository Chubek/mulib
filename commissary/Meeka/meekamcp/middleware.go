package meekamcp

import "context"

// RequestInfo describes the request currently traversing the middleware chain.
type RequestInfo struct {
	Method    string         // e.g. "tools/call", "resources/read"
	Target    string         // Tool name or Resource URI
	SessionID string         // ID assigned to the current transport peer
	Metadata  map[string]any // Inter-middleware baggage
}

// HandlerFunc is the terminal (or next) unit in the middleware chain.
type HandlerFunc func(ctx context.Context, req RequestInfo, payload any) (any, error)

// Middleware wraps a HandlerFunc with cross-cutting behavior.
type Middleware func(next HandlerFunc) HandlerFunc

// chainMiddleware composes middleware in registration order, outermost first.
func chainMiddleware(chain []Middleware, terminal HandlerFunc) HandlerFunc {
	h := terminal
	for i := len(chain) - 1; i >= 0; i-- {
		h = chain[i](h)
	}
	return h
}
