package meekamcp

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"sync"
)

// ResourceReadRequest is passed to a ResourceReadHandler on resources/read.
type ResourceReadRequest struct {
	URI    string
	Params map[string]string // extracted from the URI template
}

// ResourceReadHandler is called when a client issues resources/read.
type ResourceReadHandler func(ctx context.Context, req ResourceReadRequest) ([]Content, error)

// Notifier alerts subscribed clients about resource changes.
type Notifier interface {
	// NotifyUpdated alerts clients subscribed to uri that the resource content changed.
	NotifyUpdated(ctx context.Context, uri string) error
}

// ResourcePublisher receives a Notifier it may call whenever the resource changes.
type ResourcePublisher func(ctx context.Context, n Notifier) error

type resourceConfig struct {
	name        string
	description string
	mimeType    string
}

// ResourceOption configures resource registration.
type ResourceOption func(*resourceConfig)

func WithResourceName(name string) ResourceOption {
	return func(c *resourceConfig) { c.name = name }
}

func WithResourceDescription(desc string) ResourceOption {
	return func(c *resourceConfig) { c.description = desc }
}

func WithResourceMimeType(mime string) ResourceOption {
	return func(c *resourceConfig) { c.mimeType = mime }
}

// uriTemplate is a minimal RFC 6570 level-1 matcher: literal segments plus
// {param} expressions separated by '/'.
type uriTemplate struct {
	raw      string
	segments []templateSegment
	dynamic  bool
}

type templateSegment struct {
	literal string
	param   string // non-empty when the segment is a {param}
}

func parseURITemplate(raw string) uriTemplate {
	t := uriTemplate{raw: raw}
	for _, seg := range strings.Split(raw, "/") {
		if strings.HasPrefix(seg, "{") && strings.HasSuffix(seg, "}") && len(seg) > 2 {
			t.segments = append(t.segments, templateSegment{param: seg[1 : len(seg)-1]})
			t.dynamic = true
		} else {
			t.segments = append(t.segments, templateSegment{literal: seg})
		}
	}
	return t
}

// match extracts template parameters from a concrete URI.
func (t uriTemplate) match(uri string) (map[string]string, bool) {
	if !t.dynamic {
		if uri == t.raw {
			return map[string]string{}, true
		}
		return nil, false
	}
	parts := strings.Split(uri, "/")
	if len(parts) != len(t.segments) {
		return nil, false
	}
	params := map[string]string{}
	for i, seg := range t.segments {
		if seg.param != "" {
			if parts[i] == "" {
				return nil, false
			}
			params[seg.param] = parts[i]
			continue
		}
		if parts[i] != seg.literal {
			return nil, false
		}
	}
	return params, true
}

type registeredResource struct {
	template     uriTemplate
	cfg          resourceConfig
	handler      ResourceReadHandler
	subscribable bool

	subMu       sync.Mutex
	subscribers map[Transport]struct{}
}

// AddResource registers a static or dynamic resource.
// The URI may contain path parameters using the {param} RFC 6570 template syntax.
func AddResource(
	s *Server,
	uri string,
	handler ResourceReadHandler,
	opts ...ResourceOption,
) error {
	if s == nil {
		return fmt.Errorf("meekamcp: nil server")
	}
	if uri == "" {
		return fmt.Errorf("meekamcp: resource uri is required")
	}
	if handler == nil {
		return fmt.Errorf("meekamcp: resource %q: nil handler", uri)
	}
	cfg := resourceConfig{}
	for _, opt := range opts {
		opt(&cfg)
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, r := range s.resources {
		if r.template.raw == uri {
			return fmt.Errorf("meekamcp: duplicate resource %q", uri)
		}
	}
	s.resources = append(s.resources, &registeredResource{
		template:    parseURITemplate(uri),
		cfg:         cfg,
		handler:     handler,
		subscribers: map[Transport]struct{}{},
	})
	return nil
}

// AddResourceSubscription enables change notifications for a resource URI.
// publisher receives a Notifier it may call whenever the resource changes.
func AddResourceSubscription(
	s *Server,
	uri string,
	publisher ResourcePublisher,
) error {
	if s == nil {
		return fmt.Errorf("meekamcp: nil server")
	}
	s.mu.Lock()
	var target *registeredResource
	for _, r := range s.resources {
		if r.template.raw == uri {
			target = r
			break
		}
	}
	if target == nil {
		s.mu.Unlock()
		return fmt.Errorf("meekamcp: cannot subscribe: unknown resource %q", uri)
	}
	target.subscribable = true
	s.mu.Unlock()
	if publisher == nil {
		return nil
	}
	return publisher(context.Background(), &resourceNotifier{server: s})
}

// resourceNotifier fans notifications out to subscribed transports.
type resourceNotifier struct {
	server *Server
}

func (n *resourceNotifier) NotifyUpdated(ctx context.Context, uri string) error {
	n.server.mu.RLock()
	resources := append([]*registeredResource(nil), n.server.resources...)
	n.server.mu.RUnlock()
	note, err := newNotification("notifications/resources/updated", map[string]any{"uri": uri})
	if err != nil {
		return err
	}
	var firstErr error
	for _, r := range resources {
		if _, ok := r.template.match(uri); !ok && r.template.raw != uri {
			continue
		}
		r.subMu.Lock()
		for t := range r.subscribers {
			if err := t.Send(ctx, note); err != nil && firstErr == nil {
				firstErr = err
			}
		}
		r.subMu.Unlock()
	}
	return firstErr
}

// --- server-side resource endpoints ---

type resourceInfo struct {
	URI         string `json:"uri,omitempty"`
	URITemplate string `json:"uriTemplate,omitempty"`
	Name        string `json:"name,omitempty"`
	Description string `json:"description,omitempty"`
	MimeType    string `json:"mimeType,omitempty"`
}

func (r *registeredResource) info() resourceInfo {
	out := resourceInfo{
		Name:        r.cfg.name,
		Description: r.cfg.description,
		MimeType:    r.cfg.mimeType,
	}
	if r.template.dynamic {
		out.URITemplate = r.template.raw
	} else {
		out.URI = r.template.raw
	}
	if out.Name == "" {
		out.Name = r.template.raw
	}
	return out
}

func (s *Server) listResources(cursor string) (any, error) {
	s.mu.RLock()
	infos := make([]resourceInfo, 0, len(s.resources))
	for _, r := range s.resources {
		if !r.template.dynamic {
			infos = append(infos, r.info())
		}
	}
	s.mu.RUnlock()
	start, end, next, err := s.paginate(len(infos), cursor)
	if err != nil {
		return nil, err
	}
	return map[string]any{"resources": infos[start:end], "nextCursor": next}, nil
}

func (s *Server) listResourceTemplates(cursor string) (any, error) {
	s.mu.RLock()
	infos := make([]resourceInfo, 0, len(s.resources))
	for _, r := range s.resources {
		if r.template.dynamic {
			infos = append(infos, r.info())
		}
	}
	s.mu.RUnlock()
	start, end, next, err := s.paginate(len(infos), cursor)
	if err != nil {
		return nil, err
	}
	return map[string]any{"resourceTemplates": infos[start:end], "nextCursor": next}, nil
}

func (s *Server) findResource(uri string) (*registeredResource, map[string]string) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	for _, r := range s.resources {
		if params, ok := r.template.match(uri); ok {
			return r, params
		}
	}
	return nil, nil
}

func (s *Server) readResource(ctx context.Context, info RequestInfo, uri string) (any, error) {
	res, params := s.findResource(uri)
	if res == nil {
		return nil, NewResourceNotFoundError(uri)
	}
	terminal := func(ctx context.Context, _ RequestInfo, _ any) (any, error) {
		return res.handler(ctx, ResourceReadRequest{URI: uri, Params: params})
	}
	out, err := s.runChain(ctx, info, nil, nil, terminal)
	if err != nil {
		return nil, asMCPError(err, CodeInternalError)
	}
	items, _ := out.([]Content)
	contents := make([]json.RawMessage, 0, len(items))
	for _, c := range items {
		raw, err := marshalResourceContent(uri, res.cfg.mimeType, c)
		if err != nil {
			return nil, err
		}
		contents = append(contents, raw)
	}
	return map[string]any{"contents": contents}, nil
}

// marshalResourceContent shapes content for the resources/read reply, which
// requires uri (and optionally mimeType) on each item.
func marshalResourceContent(uri, mime string, c Content) (json.RawMessage, error) {
	raw, err := marshalContent(c)
	if err != nil {
		return nil, err
	}
	var m map[string]any
	if err := json.Unmarshal(raw, &m); err != nil {
		return nil, err
	}
	if _, ok := m["uri"]; !ok {
		m["uri"] = uri
	}
	if _, ok := m["mimeType"]; !ok && mime != "" {
		m["mimeType"] = mime
	}
	return json.Marshal(m)
}

func (s *Server) subscribeResource(_ context.Context, uri string, t Transport) (any, error) {
	res, _ := s.findResource(uri)
	if res == nil {
		return nil, NewResourceNotFoundError(uri)
	}
	if !res.subscribable {
		return nil, NewInvalidParamsError(fmt.Sprintf("resource %q does not support subscriptions", uri), nil)
	}
	if t != nil {
		res.subMu.Lock()
		res.subscribers[t] = struct{}{}
		res.subMu.Unlock()
	}
	return map[string]any{}, nil
}
