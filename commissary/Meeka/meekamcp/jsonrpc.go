package meekamcp

import "encoding/json"

const (
	// JSONRPCVersion is the version string placed in every frame.
	JSONRPCVersion = "2.0"
	// ProtocolVersion is the MCP revision this library targets.
	ProtocolVersion = "2024-11-05"
)

// JSONRPCMessage is a single JSON-RPC 2.0 frame (request, response, or notification).
type JSONRPCMessage struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method,omitempty"`
	Params  json.RawMessage `json:"params,omitempty"`
	Result  json.RawMessage `json:"result,omitempty"`
	Error   *MCPError       `json:"error,omitempty"`
}

// IsRequest reports whether the frame expects a response.
func (m *JSONRPCMessage) IsRequest() bool {
	return m.Method != "" && len(m.ID) > 0
}

// IsNotification reports whether the frame is a fire-and-forget notification.
func (m *JSONRPCMessage) IsNotification() bool {
	return m.Method != "" && len(m.ID) == 0
}

func newResponse(id json.RawMessage, result any) (*JSONRPCMessage, error) {
	raw, err := json.Marshal(result)
	if err != nil {
		return nil, err
	}
	return &JSONRPCMessage{JSONRPC: JSONRPCVersion, ID: id, Result: raw}, nil
}

func newErrorResponse(id json.RawMessage, err *MCPError) *JSONRPCMessage {
	return &JSONRPCMessage{JSONRPC: JSONRPCVersion, ID: id, Error: err}
}

func newNotification(method string, params any) (*JSONRPCMessage, error) {
	raw, err := json.Marshal(params)
	if err != nil {
		return nil, err
	}
	return &JSONRPCMessage{JSONRPC: JSONRPCVersion, Method: method, Params: raw}, nil
}
