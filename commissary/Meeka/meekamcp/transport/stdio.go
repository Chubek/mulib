// Package transport provides the built-in MeekaMCP transports: stdio,
// HTTP+SSE, and streamable HTTP.
package transport

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"io"
	"os"
	"sync"

	"hashbrownies.ir/meeka/meekamcp"
)

type stdioConfig struct {
	in          io.Reader
	out         io.Writer
	guardStdout bool
}

// StdioOption configures the stdio transport.
type StdioOption func(*stdioConfig)

// WithStdioStreams overrides the reader/writer pair (mainly for tests).
func WithStdioStreams(in io.Reader, out io.Writer) StdioOption {
	return func(c *stdioConfig) {
		c.in = in
		c.out = out
	}
}

// WithStdoutGuard redirects os.Stdout to os.Stderr so rogue prints cannot
// corrupt protocol framing; the transport keeps an isolated handle.
func WithStdoutGuard() StdioOption {
	return func(c *stdioConfig) { c.guardStdout = true }
}

// stdioTransport speaks newline-delimited JSON-RPC over stdin/stdout.
type stdioTransport struct {
	in  io.Reader
	out io.Writer

	writeMu sync.Mutex
	closeMu sync.Mutex
	closed  bool
	closeFn func() error
}

// NewStdioTransport builds a transport that listens on os.Stdin and writes
// frames over os.Stdout using newline-delimited JSON-RPC messages.
func NewStdioTransport(opts ...StdioOption) meekamcp.Transport {
	cfg := stdioConfig{in: os.Stdin, out: os.Stdout}
	for _, opt := range opts {
		opt(&cfg)
	}
	t := &stdioTransport{in: cfg.in, out: cfg.out}
	if cfg.guardStdout && cfg.out == os.Stdout {
		// Keep the original handle for the protocol; rogue writes through
		// the os.Stdout package variable go to stderr instead.
		protocol := os.Stdout
		t.out = protocol
		os.Stdout = os.Stderr
	}
	return t
}

func (t *stdioTransport) Start(ctx context.Context, handler meekamcp.MessageHandler) error {
	scanner := bufio.NewScanner(t.in)
	scanner.Buffer(make([]byte, 0, 64*1024), 16*1024*1024)
	for scanner.Scan() {
		if err := ctx.Err(); err != nil {
			return err
		}
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}
		var msg meekamcp.JSONRPCMessage
		if err := json.Unmarshal(line, &msg); err != nil {
			resp := &meekamcp.JSONRPCMessage{
				JSONRPC: meekamcp.JSONRPCVersion,
				Error:   &meekamcp.MCPError{Code: meekamcp.CodeParseError, Message: err.Error()},
			}
			if err := t.Send(ctx, resp); err != nil {
				return err
			}
			continue
		}
		resp, err := handler(ctx, &msg)
		if err != nil {
			return err
		}
		if resp != nil {
			if err := t.Send(ctx, resp); err != nil {
				return err
			}
		}
	}
	if err := scanner.Err(); err != nil && !errors.Is(err, io.EOF) {
		return err
	}
	return nil
}

func (t *stdioTransport) Send(_ context.Context, msg *meekamcp.JSONRPCMessage) error {
	raw, err := json.Marshal(msg)
	if err != nil {
		return err
	}
	t.writeMu.Lock()
	defer t.writeMu.Unlock()
	if _, err := t.out.Write(raw); err != nil {
		return err
	}
	_, err = t.out.Write([]byte{'\n'})
	return err
}

func (t *stdioTransport) Close() error {
	t.closeMu.Lock()
	defer t.closeMu.Unlock()
	if t.closed {
		return nil
	}
	t.closed = true
	if t.closeFn != nil {
		return t.closeFn()
	}
	if c, ok := t.in.(io.Closer); ok && t.in != os.Stdin {
		return c.Close()
	}
	return nil
}
