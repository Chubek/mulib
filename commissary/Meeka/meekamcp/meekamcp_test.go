package meekamcp_test

import (
	"context"
	"fmt"
	"strings"
	"testing"
	"time"

	"hashbrownies.ir/meeka/meekamcp"
	"hashbrownies.ir/meeka/meekamcp/middleware"
	meekamcptest "hashbrownies.ir/meeka/meekamcp/testing"
)

type calcInput struct {
	A  float64 `json:"a" doc:"First operand" jsonschema:"required"`
	B  float64 `json:"b" doc:"Second operand" jsonschema:"required"`
	Op string  `json:"op" doc:"Operator" enum:"add,sub,mul,div" jsonschema:"required"`
}

func newCalcServer(t *testing.T) *meekamcp.Server {
	t.Helper()
	srv := meekamcp.NewServer(meekamcp.ServerInfo{Name: "test", Version: "0.0.1"})
	err := meekamcp.AddTool(srv, "calculate", func(_ context.Context, in calcInput) (meekamcp.TextContent, error) {
		switch in.Op {
		case "add":
			return meekamcp.TextContent{Text: fmt.Sprintf("%g", in.A+in.B)}, nil
		case "div":
			if in.B == 0 {
				return meekamcp.TextContent{}, fmt.Errorf("division by zero")
			}
			return meekamcp.TextContent{Text: fmt.Sprintf("%g", in.A/in.B)}, nil
		default:
			return meekamcp.TextContent{}, fmt.Errorf("unknown op %q", in.Op)
		}
	}, meekamcp.WithToolDescription("Basic arithmetic"))
	if err != nil {
		t.Fatal(err)
	}
	return srv
}

func TestToolCallAndSchema(t *testing.T) {
	srv := newCalcServer(t)
	client := meekamcptest.NewTestClient(srv)
	defer client.Close()

	tools, err := client.ListTools(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if len(tools) != 1 || tools[0].Name != "calculate" {
		t.Fatalf("unexpected tools: %+v", tools)
	}
	schema := string(tools[0].InputSchema)
	for _, want := range []string{`"required"`, `"enum"`, `"First operand"`} {
		if !strings.Contains(schema, want) {
			t.Fatalf("schema missing %s: %s", want, schema)
		}
	}

	out, err := client.CallTool(context.Background(), "calculate", map[string]any{"a": 2, "b": 3, "op": "add"})
	if err != nil {
		t.Fatal(err)
	}
	if len(out) != 1 || out[0].(meekamcp.TextContent).Text != "5" {
		t.Fatalf("unexpected result: %+v", out)
	}
}

func TestToolErrorBecomesIsError(t *testing.T) {
	srv := newCalcServer(t)
	client := meekamcptest.NewTestClient(srv)
	defer client.Close()

	out, err := client.CallTool(context.Background(), "calculate", map[string]any{"a": 1, "b": 0, "op": "div"})
	if err == nil {
		t.Fatalf("expected tool error, got %+v", out)
	}
	if !strings.Contains(err.Error(), "division by zero") {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestStreamingTool(t *testing.T) {
	srv := meekamcp.NewServer(meekamcp.ServerInfo{Name: "s", Version: "1"})
	type in struct {
		Count int `json:"count" default:"3"`
	}
	err := meekamcp.AddStreamingTool(srv, "ticks", func(ctx context.Context, input in, stream meekamcp.ToolStream[meekamcp.TextContent]) error {
		for i := 0; i < input.Count; i++ {
			if err := stream.Write(ctx, meekamcp.TextContent{Text: fmt.Sprintf("tick %d", i)}); err != nil {
				return err
			}
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	client := meekamcptest.NewTestClient(srv)
	defer client.Close()

	out, err := client.CallTool(context.Background(), "ticks", map[string]any{"count": 2})
	if err != nil {
		t.Fatal(err)
	}
	if len(out) != 2 {
		t.Fatalf("expected 2 items, got %+v", out)
	}
	if notes := client.Notifications(); len(notes) != 2 {
		t.Fatalf("expected 2 streamed notifications, got %d", len(notes))
	}
}

func TestResourceTemplates(t *testing.T) {
	srv := meekamcp.NewServer(meekamcp.ServerInfo{Name: "s", Version: "1"})
	err := meekamcp.AddResource(srv, "system://stats/{category}", func(_ context.Context, req meekamcp.ResourceReadRequest) ([]meekamcp.Content, error) {
		if req.Params["category"] != "memory" {
			return nil, meekamcp.NewResourceNotFoundError(req.URI)
		}
		return []meekamcp.Content{meekamcp.TextContent{Text: `{"allocated_mb": 42.5}`}}, nil
	}, meekamcp.WithResourceDescription("Diag snapshots"))
	if err != nil {
		t.Fatal(err)
	}
	client := meekamcptest.NewTestClient(srv)
	defer client.Close()

	out, err := client.ReadResource(context.Background(), "system://stats/memory")
	if err != nil {
		t.Fatal(err)
	}
	if len(out) != 1 {
		t.Fatalf("unexpected contents: %+v", out)
	}
	if _, err := client.ReadResource(context.Background(), "system://stats/cpu"); err == nil {
		t.Fatal("expected error for unknown category")
	}
	if _, err := client.ReadResource(context.Background(), "nosuch://uri"); err == nil {
		t.Fatal("expected resource-not-found error")
	}
}

func TestPrompts(t *testing.T) {
	srv := meekamcp.NewServer(meekamcp.ServerInfo{Name: "s", Version: "1"})
	type promptArgs struct {
		Topic string `json:"topic" doc:"Subject" jsonschema:"required"`
	}
	err := meekamcp.AddPrompt(srv, "explain", func(_ context.Context, args promptArgs) (*meekamcp.PromptResult, error) {
		return &meekamcp.PromptResult{
			Description: "Explains a topic",
			Messages: []meekamcp.PromptMessage{
				{Role: meekamcp.RoleUser, Content: meekamcp.TextContent{Text: "Explain " + args.Topic}},
			},
		}, nil
	}, meekamcp.WithPromptDescription("Explainer"))
	if err != nil {
		t.Fatal(err)
	}
	client := meekamcptest.NewTestClient(srv)
	defer client.Close()

	res, err := client.GetPrompt(context.Background(), "explain", map[string]any{"topic": "MCP"})
	if err != nil {
		t.Fatal(err)
	}
	if len(res.Messages) != 1 || res.Messages[0].Content.(meekamcp.TextContent).Text != "Explain MCP" {
		t.Fatalf("unexpected prompt result: %+v", res)
	}
}

func TestMiddlewareOrderAndRecovery(t *testing.T) {
	srv := meekamcp.NewServer(meekamcp.ServerInfo{Name: "s", Version: "1"})
	srv.UseMiddleware(middleware.Recovery(false), middleware.Timeout(2*time.Second))
	type empty struct{}
	err := meekamcp.AddTool(srv, "boom", func(context.Context, empty) (meekamcp.TextContent, error) {
		panic("kaboom")
	})
	if err != nil {
		t.Fatal(err)
	}
	client := meekamcptest.NewTestClient(srv)
	defer client.Close()

	_, err = client.CallTool(context.Background(), "boom", nil)
	if err == nil || !strings.Contains(err.Error(), "kaboom") {
		t.Fatalf("expected recovered panic error, got %v", err)
	}
}

func TestPagination(t *testing.T) {
	srv := meekamcp.NewServer(meekamcp.ServerInfo{Name: "s", Version: "1"}, meekamcp.WithPaginationLimit(2))
	type empty struct{}
	for _, name := range []string{"b", "a", "c"} {
		if err := meekamcp.AddTool(srv, name, func(context.Context, empty) (meekamcp.TextContent, error) {
			return meekamcp.TextContent{Text: "ok"}, nil
		}); err != nil {
			t.Fatal(err)
		}
	}
	client := meekamcptest.NewTestClient(srv)
	defer client.Close()

	tools, err := client.ListTools(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if len(tools) != 3 || tools[0].Name != "a" || tools[2].Name != "c" {
		t.Fatalf("unexpected paginated tools: %+v", tools)
	}
}
