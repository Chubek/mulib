package meekamcp

import "encoding/json"

// Content represents an MCP content payload.
type Content interface {
	ContentType() string
}

// TextContent contains arbitrary UTF-8 text.
type TextContent struct {
	Text string `json:"text"`
}

func (t TextContent) ContentType() string { return "text" }

// ImageContent encapsulates binary visual payloads.
type ImageContent struct {
	Data     []byte `json:"data"` // base64 encoded when serialized over wire
	MIMEType string `json:"mimeType"`
}

func (i ImageContent) ContentType() string { return "image" }

// EmbeddedResourceContent wraps resource blobs inlined directly into a tool result.
type EmbeddedResourceContent struct {
	URI      string `json:"uri"`
	MIMEType string `json:"mimeType,omitempty"`
	Blob     []byte `json:"blob,omitempty"`
	Text     string `json:"text,omitempty"`
}

func (e EmbeddedResourceContent) ContentType() string { return "resource" }

// marshalContent serializes a Content value with its wire "type" discriminator.
func marshalContent(c Content) (json.RawMessage, error) {
	body, err := json.Marshal(c)
	if err != nil {
		return nil, err
	}
	var m map[string]any
	if err := json.Unmarshal(body, &m); err != nil {
		return nil, err
	}
	if m == nil {
		m = map[string]any{}
	}
	m["type"] = c.ContentType()
	return json.Marshal(m)
}

func marshalContents(items []Content) ([]json.RawMessage, error) {
	out := make([]json.RawMessage, 0, len(items))
	for _, c := range items {
		raw, err := marshalContent(c)
		if err != nil {
			return nil, err
		}
		out = append(out, raw)
	}
	return out, nil
}

// decodeContent converts a decoded wire map back into a typed Content value.
func decodeContent(raw json.RawMessage) (Content, error) {
	var probe struct {
		Type string `json:"type"`
	}
	if err := json.Unmarshal(raw, &probe); err != nil {
		return nil, err
	}
	switch probe.Type {
	case "image":
		var c ImageContent
		if err := json.Unmarshal(raw, &c); err != nil {
			return nil, err
		}
		return c, nil
	case "resource":
		var c EmbeddedResourceContent
		if err := json.Unmarshal(raw, &c); err != nil {
			return nil, err
		}
		return c, nil
	default:
		var c TextContent
		if err := json.Unmarshal(raw, &c); err != nil {
			return nil, err
		}
		return c, nil
	}
}

// DecodeContent converts a wire content object back into a typed Content
// value. It is exported for use by client-side helpers and the test harness.
func DecodeContent(raw json.RawMessage) (Content, error) {
	return decodeContent(raw)
}
