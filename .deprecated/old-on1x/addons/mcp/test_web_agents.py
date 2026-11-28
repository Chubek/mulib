"""Dedicated private graphics tests; no network, browser, or filesystem artifacts."""
from copy import deepcopy
from hashlib import sha256
from base64 import b64encode
from html.parser import HTMLParser
from importlib import import_module
from shutil import which
import subprocess
import unittest
from xml.etree import ElementTree

from core import EvidenceError, MAX_RESPONSE, encode

web = import_module("web-agents")
SVG_NS = "{http://www.w3.org/2000/svg}"


class HTMLDocument(HTMLParser):
    def __init__(self, markup):
        super().__init__()
        self.tags = []
        self.text = []
        self.literals = {"script": [], "style": []}
        self.literal_tag = None
        self.feed(markup)
        self.close()

    def handle_starttag(self, tag, attrs):
        self.tags.append((tag, dict(attrs)))
        if tag in self.literals:
            self.literal_tag = tag

    def handle_endtag(self, tag):
        if tag == self.literal_tag:
            self.literal_tag = None

    def handle_data(self, data):
        if self.literal_tag:
            self.literals[self.literal_tag].append(data)
        else:
            self.text.append(data)


class GraphicsTests(unittest.TestCase):
    def setUp(self):
        self.session = web.GraphicsSession()
        self.addCleanup(self.session.close)
        self.dot = {"type": "circle", "cx": 20, "cy": 20, "r": 15, "fill": "#0055DA"}
        self.badge = {"width": 40, "height": 40, "title": "Badge", "shapes": [self.dot]}

    def render(self, program=None, format="svg"):
        return self.session.call("render_graphics", {
            "program": self.badge if program is None else program, "format": format,
        })

    def display(self, saved):
        return self.session.call("display_graphics", {"document_id": saved["document_id"]})

    def artifact(self, saved):
        return self.session.embedded_resource(saved["document_id"])["resource"]

    def assert_error(self, code, fn, *args):
        with self.assertRaises(EvidenceError) as cm:
            fn(*args)
        self.assertEqual(cm.exception.code, code)

    def test_descriptor_examples_are_accepted_and_snapshots_are_detached(self):
        definitions = self.session.tools()
        self.assertEqual([tool["name"] for tool in definitions],
                         ["render_graphics", "measure_text", "display_graphics"])
        args = definitions[0]["inputSchema"]
        self.assertFalse(args["additionalProperties"])
        self.assertEqual(set(args["required"]), {"program", "format"})
        for example in args["examples"]:
            saved = self.session.call("render_graphics", example)
            self.assertEqual(saved["format"], example["format"])
        args["examples"].clear()
        self.assertEqual(len(web.tools()[0]["inputSchema"]["examples"]), 2)

    def test_render_svg_all_primitives_and_escape_text(self):
        literal = '</text><image href="bad"/><script>fixture</script>&'
        program = {**self.badge, "title": literal, "description": "A > B",
                   "background": "#F5DF52", "shapes": [
                       self.dot,
                       {"type": "rect", "x": 1, "y": 2, "width": 10, "height": 6, "rx": 2,
                        "fill": "#D43368", "stroke": "#352041", "stroke_width": 1.25, "opacity": 0.95},
                       {"type": "line", "x1": -1e-5, "y1": 3, "x2": 8, "y2": 9},
                       {"type": "polyline", "points": [[1, 2], [3.125, 4], [-5, 6]]},
                       {"type": "polygon", "points": [[1, 2], [3, 4], [5, 6]], "fill": "none"},
                       {"type": "text", "x": 8, "y": 9, "text": literal, "font": "serif",
                        "size": 12.25, "weight": "bold", "anchor": "middle", "fill": "#14348F"},
                   ]}
        saved = self.render(program)
        shown = self.display(saved)
        resource = self.artifact(saved)
        self.assertRegex(saved["document_id"], "^" + web.DOCUMENT_ID + "$")
        self.assertEqual(saved["canvas"], {"width": 40, "height": 40})
        self.assertEqual(saved["shape_count"], 6)
        self.assertEqual(saved["shape_counts"]["circle"], 1)
        self.assertEqual(shown["title"], literal)
        self.assertIn("client decides", shown["display"])
        self.assertIn("as an image", shown["sandbox"])
        self.assertEqual(resource["uri"], "on1x-graphics://document/" + saved["document_id"])
        self.assertEqual(resource["mimeType"], "image/svg+xml")
        self.assertEqual(saved["bytes"], len(resource["text"].encode("utf-8")))
        drawing = ElementTree.fromstring(resource["text"])
        self.assertEqual(drawing.get("viewBox"), "0 0 40 40")
        self.assertEqual(drawing.get("aria-describedby"), "drawing-desc")
        self.assertEqual([node.tag for node in drawing], [SVG_NS + name for name in (
            "title", "desc", "rect", "circle", "rect", "line", "polyline", "polygon", "text")])
        self.assertEqual(drawing[0].text, literal)
        self.assertEqual(drawing[1].text, "A > B")
        self.assertEqual(drawing[2].attrib["fill"], "#F5DF52")
        self.assertEqual(drawing[3].attrib["fill"], "#0055DA")
        self.assertEqual(drawing[4].attrib["stroke-width"], "1.25")
        self.assertEqual(drawing[5].attrib["x1"], "0")
        self.assertEqual(drawing[6].attrib["points"], "1,2 3.125,4 -5,6")
        self.assertEqual(drawing[-1].attrib["font-size"], "12.25")
        self.assertEqual(drawing[-1].text, literal)
        self.assertFalse(any("href" in key or key.startswith("on") for node in drawing.iter()
                             for key in node.attrib))

    def test_html_chart_labels_are_inert_and_csp_hashes_match(self):
        literal = '</title><script>fixture</script><img src="bad">&'
        label = '</bdi><button onclick="fixture()">X</button>'
        chart = {"title": literal, "series": [
            {"label": label, "value": 0.75}, {"label": "Other", "value": 0},
        ]}
        saved = self.render({**self.badge, "title": literal, "description": literal, "chart": chart}, "html")
        resource = self.artifact(saved)
        self.assertEqual(resource["mimeType"], "text/html")
        parsed = HTMLDocument(resource["text"])
        self.assertEqual([tag for tag, _ in parsed.tags].count("script"), 1)
        self.assertNotIn("img", [tag for tag, _ in parsed.tags])
        self.assertNotIn("button", [tag for tag, _ in parsed.tags])
        self.assertFalse(any(key.startswith("on") or key in {"src", "href", "style"}
                             for _, attrs in parsed.tags for key in attrs))
        self.assertIn(literal, parsed.text)
        self.assertIn(label, parsed.text)
        self.assertIn(("select", {"id": "chart-order"}), parsed.tags)
        self.assertIn(("label", {"for": "chart-order"}), parsed.tags)
        self.assertIn(("span", {"id": "chart-status", "role": "status"}), parsed.tags)
        self.assertIn(("p", {"id": "chart-controls", "hidden": None}), parsed.tags)
        self.assertIn(("th", {"scope": "row"}), parsed.tags)
        self.assertIn(("meter", {"class": "c0", "min": "0", "max": "0.75", "value": "0.75",
                                 "aria-hidden": "true"}), parsed.tags)
        csp = next(attrs["content"] for tag, attrs in parsed.tags
                   if tag == "meta" and attrs.get("http-equiv") == "Content-Security-Policy")
        for directive in ("default-src", "connect-src", "object-src", "base-uri", "form-action"):
            self.assertIn(f"{directive} 'none'", csp)
        self.assertNotIn("'unsafe-inline'", csp)
        for element, source in (("script", web._SCRIPT), ("style", web._STYLE)):
            self.assertEqual(parsed.literals[element], [source])
            digest = b64encode(sha256(source.encode()).digest()).decode()
            self.assertIn(f"{element}-src 'sha256-{digest}'", csp)
        shown = self.display(saved)
        self.assertIn('sandbox="allow-scripts"', shown["sandbox"])
        self.assertIn("no allow-same-origin", shown["sandbox"])
        self.assertEqual(shown["chart"]["series"], chart["series"])
        self.assertIn("sort_rows_by_value", shown["chart"]["interactions"])

    def test_drawing_only_html_has_no_script(self):
        markup = self.artifact(self.render(format="html"))["text"]
        self.assertIn("script-src 'none'", markup)
        self.assertNotIn("<script>", markup)
        self.assertIn("<svg ", markup)

    @unittest.skipUnless(which("node"), "optional trusted JavaScript behavior check requires Node")
    def test_trusted_chart_script_orders_all_rows_and_announces_changes(self):
        # Use the trusted script literal, not interpreted user data or generated markup.
        harness = """
const assert = require("node:assert/strict");
const input = [8, 2, 8].map((value, order) => ({dataset: {value, order}}));
const elements = {
  "chart-order": {addEventListener: function (type, fn) { assert.equal(type, "change"); this.change = fn; }},
  "chart-rows": {children: input, append: function (...rows) { this.children = rows; }},
  "chart-status": {textContent: ""},
  "chart-controls": {hidden: true}
};
global.document = {getElementById: id => elements[id]};
"""
        exercise = """
assert.equal(elements["chart-controls"].hidden, false);
for (const [value, expected, message] of [
  ["descending", [0, 2, 1], "highest to lowest"],
  ["ascending", [1, 0, 2], "lowest to highest"],
  ["input", [0, 1, 2], "input order"]
]) {
  const order = elements["chart-order"];
  order.value = value;
  order.change();
  assert.deepEqual(elements["chart-rows"].children.map(row => row.dataset.order), expected);
  assert.ok(elements["chart-status"].textContent.includes(message));
}
"""
        result = subprocess.run(["node", "-"], input=harness + web._SCRIPT + exercise,
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_reject_unknown_fields_raw_markup_and_invalid_geometry(self):
        invalid_shapes = [
            {"type": "script"}, {"type": "foreignObject"}, {"type": {"bad": "kind"}},
            {**self.dot, "href": "https://example.invalid"}, {**self.dot, "onclick": "fixture()"},
            {**self.dot, "fill": "url(#pattern)"}, {**self.dot, "fill": "#30F00A\n"},
            {**self.dot, "stroke": '#123456" x="7'}, {**self.dot, "r": False},
            {**self.dot, "r": "5"}, {**self.dot, "r": 0}, {**self.dot, "r": float("nan")},
            {**self.dot, "cx": float("inf")}, {**self.dot, "cx": 4097},
            {"type": "polyline", "points": [[0, 0]]},
            {"type": "polygon", "points": [[0, 0], [1, 1]]},
            {"type": "polyline", "points": [[0, 0, 1], [1, 1]]},
            {"type": "text", "x": 0, "y": 0, "text": "label", "font": "/local/file"},
            {"type": "text", "x": 0, "y": 0, "text": "label", "weight": "evil"},
            {"type": "text", "x": 0, "y": 0, "text": "bad\ud800"},
        ]
        for shape in invalid_shapes:
            with self.subTest(shape=shape):
                self.assert_error("INVALID_ARGUMENT", self.render, {**self.badge, "shapes": [shape]})
        invalid_programs = [
            "<svg/>", "print(1)", {**self.badge, "source": "fixture()"}, {**self.badge, "width": 20000},
            {**self.badge, "height": 1.25}, {**self.badge, "styles": {}}, {**self.badge, "shapes": []},
            {**self.badge, "shapes": [self.dot] * (web.MAX_SHAPES + 1)},
            {**self.badge, "description": "\n"}, {**self.badge, "title": "bad\u202e"},
        ]
        for program in invalid_programs:
            with self.subTest(program=program):
                self.assert_error("INVALID_ARGUMENT", self.render, program, "html")
        self.assert_error("INVALID_ARGUMENT", self.render, self.badge, "png")
        for chart in [
            {"series": []},
            {"series": [{"label": "X", "value": -1}]},
            {"series": [{"label": "X", "value": 1}] * (web.MAX_SERIES + 1)},
            {"series": [{"label": "X", "value": 1, "url": "https://example.invalid"}]},
            {"series": [{"label": "X", "value": 1}], "script": "fixture()"},
        ]:
            self.assert_error("INVALID_ARGUMENT", self.render, {**self.badge, "chart": chart}, "html")
        self.assert_error("INVALID_ARGUMENT", self.render,
                          {**self.badge, "chart": {"series": [{"label": "X", "value": 1}]}}, "svg")

    def test_measure_text_is_deterministic_and_explicitly_estimated(self):
        measure = lambda text, font="monospace", size=10: self.session.call(
            "measure_text", {"text": text, "font": font, "size": size})
        stats = measure("abc\na\tb")
        self.assertEqual(stats, measure("abc\na\tb"))
        self.assertEqual(stats["measurement"], "estimated")
        self.assertEqual(stats["width_px"], 30)
        self.assertEqual(stats["height_px"], 24)
        self.assertEqual(stats["line_height_px"], 12)
        self.assertEqual(stats["lines"], 2)
        self.assertEqual(measure("")["width_px"], 0)
        self.assertEqual(measure("e\u0301")["width_px"], measure("e")["width_px"])
        self.assertGreater(measure("界")["width_px"], measure("i")["width_px"])
        self.assertGreater(measure("MW", "sans-serif")["width_px"],
                           measure("il", "sans-serif")["width_px"])
        for text, font, size in [("x" * 1025, "monospace", 10), ("x", "Arial.ttf", 10),
                                 ("x", "monospace", 0), ("x", "monospace", True),
                                 ("x", "monospace", float("nan")), ("\r", "monospace", 10)]:
            self.assert_error("INVALID_ARGUMENT", measure, text, font, size)

    def test_connection_isolation_immutable_result_fifo_and_close(self):
        first = self.render()
        other_session = web.GraphicsSession()
        self.addCleanup(other_session.close)
        same_scene = other_session.call("render_graphics", {"format": "svg", "program": self.badge})
        self.assertNotEqual(first["document_id"], same_scene["document_id"])
        self.assert_error("GRAPHICS_NOT_FOUND", other_session.embedded_resource, first["document_id"])
        self.assert_error("GRAPHICS_NOT_FOUND", self.display, same_scene)
        for invalid in ("../file", "file:///etc/passwd", "on1x-graphics://document/" + first["document_id"]):
            self.assert_error("INVALID_ARGUMENT", self.session.embedded_resource, invalid)
        for _ in range(web.MAX_DOCUMENTS - 1):
            self.render()
        summary = self.display(first)
        summary["shape_counts"].clear()
        self.assertEqual(self.display(first)["shape_counts"], {"circle": 1})
        last = self.render()
        self.assert_error("GRAPHICS_NOT_FOUND", self.display, first)
        self.assertEqual(self.display(last)["title"], "Badge")
        self.session.close()
        self.assert_error("GRAPHICS_NOT_FOUND", self.display, last)
        self.assertIn("<svg", other_session.embedded_resource(same_scene["document_id"])["resource"]["text"])

    def test_document_wire_limit_and_optional_resource_fit_mcp_budget(self):
        many_chars = {"type": "text", "x": 0, "y": 0, "text": "😊" * 256}
        program = {**self.badge, "shapes": [many_chars] * 7}
        saved = self.render(program)
        shown = self.display(saved)
        resource = self.session.embedded_resource(saved["document_id"])
        wire_size = len(encode(resource["resource"]["text"]))
        self.assertGreater(wire_size, 20 * 1024)
        self.assertLessEqual(wire_size, web.MAX_DOCUMENT_WIRE_BYTES)
        response = {"jsonrpc": "2.0", "id": 123, "result": {
            "structuredContent": shown, "isError": False,
            "content": [{"type": "text", "text": encode(shown).decode()}, resource],
        }}
        self.assertLessEqual(len(encode(response)), MAX_RESPONSE)
        too_large = deepcopy(program)
        too_large["shapes"].extend([many_chars] * 2)
        self.assert_error("GRAPHICS_LIMIT", self.render, too_large)
        self.assertEqual(self.display(saved), shown)


if __name__ == "__main__":
    unittest.main()
