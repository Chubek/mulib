package meekamcp

import (
	"context"
	"encoding/json"
	"fmt"
	"reflect"
	"sort"
)

// Role identifies the author of a prompt message.
type Role string

const (
	RoleUser      Role = "user"
	RoleAssistant Role = "assistant"
	RoleSystem    Role = "system"
)

// PromptMessage is a single message inside a PromptResult.
type PromptMessage struct {
	Role    Role
	Content Content
}

// PromptResult is returned from a PromptHandler.
type PromptResult struct {
	Description string
	Messages    []PromptMessage
}

// PromptHandler expands prompt arguments into messages.
type PromptHandler[P any] func(ctx context.Context, args P) (*PromptResult, error)

type promptConfig struct {
	description string
}

// PromptOption configures prompt registration.
type PromptOption func(*promptConfig)

func WithPromptDescription(desc string) PromptOption {
	return func(c *promptConfig) { c.description = desc }
}

type promptArgument struct {
	Name        string `json:"name"`
	Description string `json:"description,omitempty"`
	Required    bool   `json:"required,omitempty"`
}

type registeredPrompt struct {
	name        string
	description string
	arguments   []promptArgument
	handler     func(ctx context.Context, args json.RawMessage) (*PromptResult, error)
}

// AddPrompt registers an interactive or templated prompt.
// Input arguments are derived from struct P via JSON tags.
func AddPrompt[P any](
	s *Server,
	name string,
	handler PromptHandler[P],
	opts ...PromptOption,
) error {
	if s == nil {
		return fmt.Errorf("meekamcp: nil server")
	}
	if name == "" {
		return fmt.Errorf("meekamcp: prompt name is required")
	}
	if handler == nil {
		return fmt.Errorf("meekamcp: prompt %q: nil handler", name)
	}
	cfg := promptConfig{}
	for _, opt := range opts {
		opt(&cfg)
	}
	args, err := promptArgumentsFor(reflect.TypeFor[P]())
	if err != nil {
		return fmt.Errorf("meekamcp: prompt %q: %w", name, err)
	}
	raw := func(ctx context.Context, rawArgs json.RawMessage) (*PromptResult, error) {
		input, err := decodeInput[P](rawArgs)
		if err != nil {
			return nil, err
		}
		return handler(ctx, input)
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.prompts[name]; exists {
		return fmt.Errorf("meekamcp: duplicate prompt %q", name)
	}
	s.prompts[name] = &registeredPrompt{
		name:        name,
		description: cfg.description,
		arguments:   args,
		handler:     raw,
	}
	return nil
}

// promptArgumentsFor lists prompt arguments derived from struct P.
func promptArgumentsFor(t reflect.Type) ([]promptArgument, error) {
	for t.Kind() == reflect.Pointer {
		t = t.Elem()
	}
	if t.Kind() != reflect.Struct {
		return nil, fmt.Errorf("prompt arguments must be a struct, got %s", t.Kind())
	}
	var out []promptArgument
	for i := 0; i < t.NumField(); i++ {
		field := t.Field(i)
		if !field.IsExported() {
			continue
		}
		name, _, skip := jsonName(field)
		if skip {
			continue
		}
		arg := promptArgument{Name: name}
		if doc := field.Tag.Get("doc"); doc != "" {
			arg.Description = doc
		} else if desc := field.Tag.Get("description"); desc != "" {
			arg.Description = desc
		}
		for _, entry := range splitTag(field.Tag.Get("jsonschema")) {
			if entry == "required" {
				arg.Required = true
			}
		}
		out = append(out, arg)
	}
	return out, nil
}

func splitTag(tag string) []string {
	if tag == "" {
		return nil
	}
	var out []string
	start := 0
	for i := 0; i <= len(tag); i++ {
		if i == len(tag) || tag[i] == ',' {
			out = append(out, tag[start:i])
			start = i + 1
		}
	}
	return out
}

// --- server-side prompt endpoints ---

type promptInfo struct {
	Name        string           `json:"name"`
	Description string           `json:"description,omitempty"`
	Arguments   []promptArgument `json:"arguments,omitempty"`
}

func (s *Server) listPrompts(cursor string) (any, error) {
	s.mu.RLock()
	names := make([]string, 0, len(s.prompts))
	for n := range s.prompts {
		names = append(names, n)
	}
	s.mu.RUnlock()
	sort.Strings(names)
	infos := make([]promptInfo, 0, len(names))
	s.mu.RLock()
	for _, n := range names {
		p := s.prompts[n]
		infos = append(infos, promptInfo{Name: p.name, Description: p.description, Arguments: p.arguments})
	}
	s.mu.RUnlock()
	start, end, next, err := s.paginate(len(infos), cursor)
	if err != nil {
		return nil, err
	}
	return map[string]any{"prompts": infos[start:end], "nextCursor": next}, nil
}

func (s *Server) getPrompt(ctx context.Context, info RequestInfo, name string, args json.RawMessage) (any, error) {
	s.mu.RLock()
	prompt, ok := s.prompts[name]
	s.mu.RUnlock()
	if !ok {
		return nil, NewMethodNotFoundError("prompt " + name)
	}
	terminal := func(ctx context.Context, _ RequestInfo, payload any) (any, error) {
		raw, _ := payload.(json.RawMessage)
		return prompt.handler(ctx, raw)
	}
	out, err := s.runChain(ctx, info, args, nil, terminal)
	if err != nil {
		return nil, asMCPError(err, CodeInternalError)
	}
	result, _ := out.(*PromptResult)
	if result == nil {
		result = &PromptResult{}
	}
	messages := make([]map[string]any, 0, len(result.Messages))
	for _, m := range result.Messages {
		raw, err := marshalContent(m.Content)
		if err != nil {
			return nil, err
		}
		messages = append(messages, map[string]any{
			"role":    m.Role,
			"content": json.RawMessage(raw),
		})
	}
	return map[string]any{
		"description": result.Description,
		"messages":    messages,
	}, nil
}
