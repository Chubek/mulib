// Package middleware provides built-in MeekaMCP middleware: logging, panic
// recovery, rate limiting, and timeouts.
package middleware

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"runtime/debug"
	"sync"
	"time"

	"hashbrownies.ir/meeka/meekamcp"
)

// Logging injects structured telemetry via log/slog.
func Logging(logger *slog.Logger) meekamcp.Middleware {
	return func(next meekamcp.HandlerFunc) meekamcp.HandlerFunc {
		return func(ctx context.Context, req meekamcp.RequestInfo, payload any) (any, error) {
			start := time.Now()
			out, err := next(ctx, req, payload)
			logger.LogAttrs(ctx, slog.LevelInfo, "meekamcp request",
				slog.String("method", req.Method),
				slog.String("target", req.Target),
				slog.String("session", req.SessionID),
				slog.Duration("elapsed", time.Since(start)),
				slog.Bool("error", err != nil),
			)
			return out, err
		}
	}
}

// Recovery converts panics into MCP internal server errors with optional stack capture.
func Recovery(dumpStack bool) meekamcp.Middleware {
	return func(next meekamcp.HandlerFunc) meekamcp.HandlerFunc {
		return func(ctx context.Context, req meekamcp.RequestInfo, payload any) (out any, err error) {
			defer func() {
				if r := recover(); r != nil {
					var data any
					if dumpStack {
						data = string(debug.Stack())
					}
					err = &meekamcp.MCPError{
						Code:    meekamcp.CodeInternalError,
						Message: fmt.Sprintf("panic: %v", r),
						Data:    data,
					}
				}
			}()
			return next(ctx, req, payload)
		}
	}
}

// tokenBucket is a minimal token-bucket rate limiter.
type tokenBucket struct {
	mu     sync.Mutex
	rps    float64
	burst  float64
	tokens float64
	last   time.Time
}

func (b *tokenBucket) allow() bool {
	b.mu.Lock()
	defer b.mu.Unlock()
	now := time.Now()
	if !b.last.IsZero() {
		b.tokens += now.Sub(b.last).Seconds() * b.rps
	} else {
		b.tokens = b.burst
	}
	if b.tokens > b.burst {
		b.tokens = b.burst
	}
	b.last = now
	if b.tokens < 1 {
		return false
	}
	b.tokens--
	return true
}

// RateLimiting bounds request throughput per connected session.
func RateLimiting(rps float64, burst int) meekamcp.Middleware {
	var mu sync.Mutex
	buckets := map[string]*tokenBucket{}
	return func(next meekamcp.HandlerFunc) meekamcp.HandlerFunc {
		return func(ctx context.Context, req meekamcp.RequestInfo, payload any) (any, error) {
			mu.Lock()
			bucket, ok := buckets[req.SessionID]
			if !ok {
				bucket = &tokenBucket{rps: rps, burst: float64(burst)}
				buckets[req.SessionID] = bucket
			}
			mu.Unlock()
			if !bucket.allow() {
				return nil, &meekamcp.MCPError{Code: meekamcp.CodeRateLimited, Message: "rate limit exceeded"}
			}
			return next(ctx, req, payload)
		}
	}
}

// Validate rejects malformed request payloads before they reach handlers.
func Validate() meekamcp.Middleware {
	return func(next meekamcp.HandlerFunc) meekamcp.HandlerFunc {
		return func(ctx context.Context, req meekamcp.RequestInfo, payload any) (any, error) {
			// Reject argument payloads that are not JSON objects before they
			// reach the typed decode step; deeper constraints are enforced by
			// the schema-derived unmarshalling in the handlers themselves.
			if raw, ok := payload.(json.RawMessage); ok && len(raw) > 0 {
				var probe map[string]any
				if err := json.Unmarshal(raw, &probe); err != nil {
					return nil, meekamcp.NewInvalidParamsError("arguments must be a JSON object", err.Error())
				}
			}
			return next(ctx, req, payload)
		}
	}
}

// Timeout imposes a strict execution deadline on tool invocations.
func Timeout(d time.Duration) meekamcp.Middleware {
	return func(next meekamcp.HandlerFunc) meekamcp.HandlerFunc {
		return func(ctx context.Context, req meekamcp.RequestInfo, payload any) (any, error) {
			ctx, cancel := context.WithTimeout(ctx, d)
			defer cancel()
			type result struct {
				out any
				err error
			}
			ch := make(chan result, 1)
			panicCh := make(chan any, 1)
			go func() {
				// Forward panics across the goroutine boundary so an outer
				// Recovery middleware can still observe them.
				defer func() {
					if r := recover(); r != nil {
						panicCh <- r
					}
				}()
				out, err := next(ctx, req, payload)
				ch <- result{out, err}
			}()
			select {
			case r := <-ch:
				return r.out, r.err
			case p := <-panicCh:
				panic(p)
			case <-ctx.Done():
				return nil, &meekamcp.MCPError{Code: meekamcp.CodeInternalError, Message: "request timed out"}
			}
		}
	}
}
