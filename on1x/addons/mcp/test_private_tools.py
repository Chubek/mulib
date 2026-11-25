"""Transport checks for per-connection MCP providers."""
from importlib import import_module
import unittest

from server import dispatch, tool_registry


class Fixture:
    def tools(self):
        return [{"name": "private_fixture", "inputSchema": {"type": "object"}}]

    def call(self, name, args):
        return {"name": name, "args": args}


class PrivateTools(unittest.TestCase):
    def test_tools_require_private_registry_and_initialization(self):
        registry = tool_registry([Fixture()])
        call = {"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                "params": {"name": "private_fixture", "arguments": {"value": 7}}}
        self.assertEqual(dispatch(None, call)["error"]["code"], -32602)
        self.assertEqual(dispatch(None, call, initialized=False, private=registry)["error"]["code"], -32002)
        result = dispatch(None, call, private=registry)["result"]
        self.assertFalse(result["isError"])
        self.assertEqual(result["structuredContent"]["args"], {"value": 7})

    def test_descriptors_are_isolated(self):
        request = {"jsonrpc": "2.0", "id": 1, "method": "tools/list"}
        def names(**kwargs):
            return {item["name"] for item in dispatch(None, request, **kwargs)["result"]["tools"]}
        self.assertNotIn("private_fixture", names())
        self.assertIn("private_fixture", names(private=tool_registry([Fixture()])))

    def test_duplicate_tools_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            tool_registry([Fixture(), Fixture()])

    def test_graphics_returns_an_embedded_resource_only_in_the_owning_session(self):
        graphics = import_module("web-agents").GraphicsSession
        first, second = (tool_registry([graphics()]) for _ in range(2))

        def call(name, args, registry):
            request = {"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                       "params": {"name": name, "arguments": args}}
            return dispatch(None, request, private=registry)["result"]

        rendered = call("render_graphics", {"format": "svg", "program": {
            "width": 60, "height": 40,
            "shapes": [{"type": "circle", "cx": 30, "cy": 20, "r": 12, "fill": "#2EA8E5"}],
        }}, first)
        self.assertFalse(rendered["isError"])
        args = {"document_id": rendered["structuredContent"]["document_id"]}
        self.assertTrue(call("display_graphics", args, second)["isError"])
        result = call("display_graphics", args, first)
        self.assertFalse(result["isError"])
        self.assertEqual(result["content"][1]["type"], "resource")
        self.assertEqual(result["content"][1]["resource"]["mimeType"], "image/svg+xml")
        self.assertIn("#2EA8E5", result["content"][1]["resource"]["text"])


if __name__ == "__main__":
    unittest.main()
