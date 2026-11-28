package meekamcp

import "context"

// MessageHandler processes one inbound frame and returns the response frame
// (nil for notifications).
type MessageHandler func(ctx context.Context, msg *JSONRPCMessage) (*JSONRPCMessage, error)

// Transport is the bidirectional message layer between server and client.
type Transport interface {
	// Start begins the message delivery loop and returns upon disconnect or
	// unrecoverable error.
	Start(ctx context.Context, handler MessageHandler) error
	// Send pushes an outbound frame over the connection.
	Send(ctx context.Context, msg *JSONRPCMessage) error
	// Close cleanly shuts down the transport channel.
	Close() error
}
