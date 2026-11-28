package meekamcp

import "fmt"

// ErrorCode enumerates JSON-RPC 2.0 and MCP-specific error codes.
type ErrorCode int

const (
	CodeParseError     ErrorCode = -32700
	CodeInvalidRequest ErrorCode = -32600
	CodeMethodNotFound ErrorCode = -32601
	CodeInvalidParams  ErrorCode = -32602
	CodeInternalError  ErrorCode = -32603

	// MCP specific codes
	CodeResourceNotFound ErrorCode = -32001
	CodeToolExecution    ErrorCode = -32002
	CodeRateLimited      ErrorCode = -32003
	CodeUnauthorized     ErrorCode = -32004
)

// MCPError is a structured error that maps directly onto a JSON-RPC error object.
type MCPError struct {
	Code    ErrorCode `json:"code"`
	Message string    `json:"message"`
	Data    any       `json:"data,omitempty"`
}

func (e *MCPError) Error() string {
	return fmt.Sprintf("mcp error: %s (code: %d)", e.Message, e.Code)
}

func NewToolError(msg string) *MCPError {
	return &MCPError{Code: CodeToolExecution, Message: msg}
}

func NewInvalidParamsError(msg string, details any) *MCPError {
	return &MCPError{Code: CodeInvalidParams, Message: msg, Data: details}
}

func NewResourceNotFoundError(uri string) *MCPError {
	return &MCPError{Code: CodeResourceNotFound, Message: fmt.Sprintf("resource not found: %s", uri)}
}

func NewMethodNotFoundError(method string) *MCPError {
	return &MCPError{Code: CodeMethodNotFound, Message: fmt.Sprintf("method not found: %s", method)}
}

// asMCPError wraps any error into an *MCPError, defaulting to CodeToolExecution.
func asMCPError(err error, fallback ErrorCode) *MCPError {
	if err == nil {
		return nil
	}
	if me, ok := err.(*MCPError); ok {
		return me
	}
	return &MCPError{Code: fallback, Message: err.Error()}
}
