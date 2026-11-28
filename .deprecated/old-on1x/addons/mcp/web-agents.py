"""Bounded declarative SVG/HTML for a single private MCP stdio connection."""
from base64 import b64encode
from copy import deepcopy
from hashlib import sha256
from html import escape
import re
from secrets import token_hex
import unicodedata

from core import encode, fail

MAX_DOCUMENTS = 8  # FIFO; reads do not extend retention.
MAX_DOCUMENT_WIRE_BYTES = 24 * 1024  # Includes JSON quoting and Unicode escaping.
MAX_SHAPES = 64
MAX_SERIES = 12
MIME_TYPES = {"svg": "image/svg+xml", "html": "text/html"}
FONTS = ("sans-serif", "serif", "monospace")
DOCUMENT_ID = r"g_[0-9a-f]{32}"
COLOR = r"(?:#[0-9A-Fa-f]{6}|none)"
DISALLOWED_TEXT = re.compile(
    r"[\x00-\x08\x0b-\x1f\x7f-\x9f\ud800-\udfff\ufffe\uffff"
    r"\u061c\u200e\u200f\u202a-\u202e\u2066-\u2069]"
)


def _object(properties, *required):
    return {"type": "object", "properties": properties, "required": list(required),
            "additionalProperties": False}


def _array(items, maximum, minimum=0):
    return {"type": "array", "items": items, "minItems": minimum, "maxItems": maximum}


def _number(low, high, integer=False):
    return {"type": "integer" if integer else "number", "minimum": low, "maximum": high}


def _text(maximum, minimum=0):
    return {"type": "string", "minLength": minimum, "maxLength": maximum}


def _choice(*options):
    return {"type": "string", "enum": list(options)}


_PAINT = {"type": "string", "pattern": f"^{COLOR}$"}
_POS = _number(-4096, 4096)
_LENGTH = _number(0.001, 4096)
_SIZE = _number(1, 128)
_FONT = _choice(*FONTS)
_XY = {"x": _POS, "y": _POS}
_POINTS = _array(_array(_POS, 2, 2), 24, 2)
_STROKE = {"stroke": _PAINT, "stroke_width": _number(0.001, 32)}
_FILL = {"fill": _PAINT, **_STROKE}


def _shape(kind, fields, required, style):
    return _object({"type": {"type": "string", "const": kind}, **fields, **style,
                    "opacity": _number(0, 1)}, "type", *required)


_SHAPES = {
    "rect": _shape("rect", {**_XY, "width": _LENGTH, "height": _LENGTH, "rx": _number(0, 512)},
                   ("x", "y", "width", "height"), _FILL),
    "circle": _shape("circle", {"cx": _POS, "cy": _POS, "r": _LENGTH}, ("cx", "cy", "r"), _FILL),
    "line": _shape("line", {k: _POS for k in ("x1", "y1", "x2", "y2")},
                   ("x1", "y1", "x2", "y2"), _STROKE),
    "polyline": _shape("polyline", {"points": _POINTS}, ("points",), _FILL),
    "polygon": _shape("polygon", {"points": {**_POINTS, "minItems": 3}}, ("points",), _FILL),
    "text": _shape("text", {**_XY, "text": _text(256, 1), "size": _SIZE, "font": _FONT,
                            "weight": _choice("normal", "bold"),
                            "anchor": _choice("start", "middle", "end")}, ("x", "y", "text"),
                   {"fill": _PAINT}),
}
_CHART = _object({
    "title": _text(96, 1),
    "series": _array(_object({"label": _text(60, 1), "value": _number(0, 1000000)},
                             "label", "value"), MAX_SERIES, 1),
}, "series")
_PROGRAM = _object({
    "width": {**_number(1, 2048, integer=True), "description": "SVG canvas width in CSS pixels."},
    "height": {**_number(1, 2048, integer=True), "description": "SVG canvas height in CSS pixels."},
    "title": _text(96, 1),
    "description": _text(256),
    "background": _PAINT,
    "shapes": _array({"oneOf": list(_SHAPES.values())}, MAX_SHAPES),
    "chart": {**_CHART, "description": "Available only with format 'html'."},
}, "width", "height")
_PROGRAM["anyOf"] = [
    {"required": ["shapes"], "properties": {"shapes": {"minItems": 1}}},
    {"required": ["chart"]},
]
_SCHEMAS = {
    "render_graphics": _object({"program": _PROGRAM, "format": _choice("svg", "html")},
                               "program", "format"),
    "measure_text": _object({"text": _text(1024), "font": _FONT, "size": _SIZE},
                            "text", "font", "size"),
    "display_graphics": _object(
        {"document_id": {"type": "string", "pattern": f"^{DOCUMENT_ID}$"}},
        "document_id"),
}
_EXAMPLES = [
    {"format": "svg", "program": {"width": 240, "height": 100, "title": "Color badge",
     "background": "#18243D", "shapes": [
         {"type": "circle", "cx": 48, "cy": 50, "r": 26, "fill": "#FFB347"},
         {"type": "text", "x": 90, "y": 56, "text": "Ready", "size": 24,
          "weight": "bold", "fill": "#F6F9FF"},
     ]}},
    {"format": "html", "program": {"width": 420, "height": 180, "title": "Build times",
     "chart": {"title": "Elapsed seconds", "series": [
         {"label": "Parser", "value": 4}, {"label": "Backend", "value": 11},
     ]}}},
]
_SCHEMAS["render_graphics"]["examples"] = _EXAMPLES
_DESCRIPTIONS = {
    "render_graphics": (
        "Save a bounded declarative drawing in this private connection. Use numeric geometry and "
        "six-digit hex colors, not source code or markup. Numbers render to three fractional digits. "
        "HTML can include a keyboard-sortable "
        "bar chart of nonnegative values. Returns an opaque ID; display_graphics retrieves the document."
    ),
    "measure_text": (
        "Estimate text advance and line height in CSS pixels with generic fonts, not actual "
        "glyph bounds or browser/font measurement. Newlines and tab stops are supported."
    ),
    "display_graphics": (
        "Return a plain summary and an embedded portable SVG/HTML document by its "
        "private connection ID. This does not open a window or guarantee client rendering."
    ),
}


def tools():
    return [{"name": name, "description": _DESCRIPTIONS[name], "inputSchema": deepcopy(schema),
             "annotations": {"readOnlyHint": name != "render_graphics",
                             "destructiveHint": False, "openWorldHint": False}}
            for name, schema in _SCHEMAS.items()]


def _expect(condition, where, message):
    if not condition:
        fail("INVALID_ARGUMENT", f"{where}: {message}")


def _validate(value, schema, where, multiline=False):
    # A validator for the fixed schemas above, not for arbitrary JSON Schema.
    if "oneOf" in schema:
        kind = value.get("type") if type(value) is dict else None
        _expect(type(kind) is str and kind in _SHAPES, where, "unknown drawing primitive.")
        return _validate(value, _SHAPES[kind], where)
    expected = schema["type"]
    if expected == "object":
        _expect(type(value) is dict, where, "expected an object.")
        properties = schema["properties"]
        _expect(value.keys() <= properties.keys(), where, "unknown field.")
        for key in schema["required"]:
            _expect(key in value, where, f"missing required field {key}.")
        for key, field in properties.items():
            if key in value:
                _validate(value[key], field, f"{where}.{key}", multiline)
    elif expected == "array":
        low, high = schema["minItems"], schema["maxItems"]
        _expect(type(value) is list and low <= len(value) <= high, where,
                f"expected an array with {low} to {high} entries.")
        for i, item in enumerate(value):
            _validate(item, schema["items"], f"{where}[{i}]")
    elif expected == "string":
        _expect(type(value) is str, where, "expected a string.")
        low, high = schema.get("minLength", 0), schema.get("maxLength", 96)
        _expect(low <= len(value) <= high, where, f"expected {low} to {high} characters.")
        _expect(not DISALLOWED_TEXT.search(value) and (multiline or not ("\n" in value or "\t" in value)),
                where, "unsupported control character.")
        if "pattern" in schema:
            _expect(re.fullmatch(schema["pattern"], value) is not None, where,
                    "invalid format (colors must be six-digit hex or 'none')." if where.endswith(
                        (".fill", ".stroke", ".background")) else "invalid format.")
        if "enum" in schema:
            _expect(value in schema["enum"], where, "expected one of: " + ", ".join(schema["enum"]) + ".")
        if "const" in schema:
            _expect(value == schema["const"], where, "unexpected value.")
    else:
        low, high = schema["minimum"], schema["maximum"]
        _expect(type(value) in (int, float) and low <= value <= high
                and (expected != "integer" or type(value) is int or value.is_integer()), where,
                f"expected a finite {expected} between {low} and {high}.")


def _num(value):
    """Decimal attribute/visible number at fixed precision, without exponent or negative zero."""
    number = f"{value:.3f}".rstrip("0").rstrip(".")
    return "0" if number == "-0" else number


def _tag_attrs(attrs):
    return "".join(f' {key}="{escape(value if type(value) is str else _num(value), quote=True)}"'
                   for key, value in attrs.items())


def _svg_shape(shape):
    kind = shape["type"]
    if kind == "text":
        attrs = {"x": shape["x"], "y": shape["y"], "fill": shape.get("fill", "#17243B"),
                 "font-family": shape.get("font", "sans-serif"), "font-size": shape.get("size", 16),
                 "font-weight": shape.get("weight", "normal"),
                 "text-anchor": shape.get("anchor", "start"), "xml:space": "preserve"}
        attrs["opacity"] = shape.get("opacity", 1)
        return f'<text{_tag_attrs(attrs)}>{escape(shape["text"])}</text>'
    if kind in {"polygon", "polyline"}:
        attrs = {"points": " ".join(f"{_num(x)},{_num(y)}" for x, y in shape["points"])}
    else:
        attrs = {k: shape[k] for k in _SHAPES[kind]["required"] if k != "type"}
    if "rx" in shape:
        attrs["rx"] = shape["rx"]
    color = "#1756A9" if kind in {"line", "polyline"} else "none"
    attrs["stroke"] = shape.get("stroke", color)
    if kind != "line":
        attrs["fill"] = shape.get("fill", "none" if kind == "polyline" else "#008A82")
    attrs["stroke-width"] = shape.get("stroke_width", 2)
    attrs["opacity"] = shape.get("opacity", 1)
    return f"<{kind}{_tag_attrs(attrs)}/>"


def _svg(program):
    w, h = int(program["width"]), int(program["height"])
    description = program.get("description", "")
    describedby = ' aria-describedby="drawing-desc"' if description else ""
    output = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}"'
        f' role="img" aria-labelledby="drawing-title"{describedby}>',
        f'<title id="drawing-title">{escape(program.get("title", "Drawing"))}</title>',
    ]
    if description:
        output.append(f'<desc id="drawing-desc">{escape(description)}</desc>')
    fill = escape(program.get("background", "#FFFFFF"), quote=True)
    output.append(f'<rect width="{w}" height="{h}" fill="{fill}"/>')
    output.extend(_svg_shape(shape) for shape in program.get("shapes", []))
    return "\n".join(output + ["</svg>"])


# Neither of these literals contains, reads, or evaluates user-supplied code.
# The script sorts already-built DOM nodes on a native, keyboard-accessible select.
_STYLE = """\
:root{color-scheme:light;font:16px system-ui,sans-serif;color:#17243b;background:#eff5fc}
body{margin:0;padding:1rem}
main{max-width:65rem;margin:auto;padding:1rem;background:#fff;border-radius:.75rem}
h1{margin:0}h1,h2,p,th{overflow-wrap:anywhere}
svg{display:block;max-width:100%;height:auto;margin:1rem 0;border:1px solid #c4d6ec}
table{border-collapse:collapse;width:100%;table-layout:fixed}
th,td{text-align:start;padding:.45rem;border-bottom:1px solid #c4d6ec}
thead th:last-child{width:24%}td:last-child{font-variant-numeric:tabular-nums}
bdi{overflow-wrap:anywhere}
meter{width:100%;accent-color:var(--bar)}meter::-webkit-meter-optimum-value{background:var(--bar)}
meter::-moz-meter-bar{background:var(--bar)}
.c0{--bar:#176ad1}.c1{--bar:#007d70}.c2{--bar:#9540c3}
.c3{--bar:#a64418}.c4{--bar:#bb2463}.c5{--bar:#386900}
select{color:inherit;background:#fff;font:inherit;padding:.25rem}
:focus-visible{outline:3px solid #176ad1;outline-offset:3px}
"""
_SCRIPT = """\
(() => {
  "use strict";
  const order = document.getElementById("chart-order");
  const rows = document.getElementById("chart-rows");
  const status = document.getElementById("chart-status");
  order.addEventListener("change", () => {
    const direction = order.value === "descending" ? -1 : order.value === "ascending" ? 1 : 0;
    const sorted = Array.from(rows.children).sort((a, b) =>
      direction * (Number(a.dataset.value) - Number(b.dataset.value)) ||
      Number(a.dataset.order) - Number(b.dataset.order));
    rows.append(...sorted);
    status.textContent = direction === -1 ? "Rows sorted from highest to lowest." :
      direction === 1 ? "Rows sorted from lowest to highest." : "Rows restored to input order.";
  });
  document.getElementById("chart-controls").hidden = false;
})();
"""


def _csp_hash(source):
    return "'sha256-" + b64encode(sha256(source.encode("ascii")).digest()).decode("ascii") + "'"


def _chart_html(chart):
    title = escape(chart.get("title", "Data chart"))
    values = [_num(row["value"]) for row in chart["series"]]
    maximum = max(values, key=float)
    if maximum == "0":
        maximum = "1"
    output = [
        '<section aria-labelledby="chart-title">',
        f'<h2 id="chart-title">{title}</h2>',
        '<p id="chart-controls" hidden><label for="chart-order">Sort rows:</label> '
        '<select id="chart-order"><option value="input">Input order</option>'
        '<option value="descending">Highest first</option><option value="ascending">Lowest first</option>'
        '</select> <span id="chart-status" role="status"></span></p>',
        '<table aria-labelledby="chart-title"><thead><tr><th scope="col">Label</th>'
        '<th scope="col">Relative bar</th><th scope="col">Value</th></tr></thead><tbody id="chart-rows">',
    ]
    for i, (row, value) in enumerate(zip(chart["series"], values)):
        output.append(
            f'<tr data-order="{i}" data-value="{value}"><th scope="row"><bdi>{escape(row["label"])}</bdi></th>'
            f'<td><meter class="c{i % 6}" min="0" max="{maximum}" value="{value}" '
            f'aria-hidden="true">{value}</meter></td><td>{value}</td></tr>'
        )
    return "\n".join(output + ["</tbody></table></section>"])


def _html(program):
    chart = program.get("chart")
    title = escape(program.get("title", "Drawing"))
    script_policy = _csp_hash(_SCRIPT) if chart else "'none'"
    csp = (f"default-src 'none'; script-src {script_policy}; style-src {_csp_hash(_STYLE)}; "
           "connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'")
    # Double-quoted attribute; these policy tokens are trusted and have no double quote.
    output = [
        '<!doctype html><html lang="en"><head><meta charset="utf-8">',
        f'<meta http-equiv="Content-Security-Policy" content="{csp}">',
        '<meta name="referrer" content="no-referrer">',
        '<meta name="viewport" content="width=device-width, initial-scale=1">',
        f"<title>{title}</title><style>{_STYLE}</style></head>",
        f"<body><main><h1>{title}</h1>",
    ]
    if program.get("description"):
        output.append(f'<p>{escape(program["description"])}</p>')
    if program.get("shapes"):
        output.append(_svg(program))
    if chart:
        output.append(_chart_html(chart))
    output.append("</main>")
    if chart:
        output.append(f"<script>{_SCRIPT}</script>")
    return "\n".join(output + ["</body></html>"])


def _advance(character, font):
    category = unicodedata.category(character)
    if category in {"Mn", "Me", "Cf"}:
        return 0
    if unicodedata.east_asian_width(character) in {"W", "F"}:
        return 100
    if font == "monospace":
        return 60
    if character.isspace():
        return 25 if font == "serif" else 28
    if character in "ilI!.,:;'`|":
        return 29
    if character in "MW@%#mw":
        return 85
    if character.isupper():
        return 66
    return 50 if font == "serif" else 55


def _measure(text, font, size):
    # Hundredths-of-em arithmetic until final conversion, no host fonts or shaping.
    tab = 4 * _advance(" ", font)
    widths = []
    for line in text.split("\n"):
        advance = 0
        for character in line:
            advance = (advance // tab + 1) * tab if character == "\t" else advance + _advance(character, font)
        widths.append(advance)
    return {
        "font": font, "size_px": size, "measurement": "estimated", "algorithm": "heuristic-v1",
        "unicode_version": unicodedata.unidata_version, "lines": len(widths),
        "width_px": round(max(widths) * size / 100, 3), "line_height_px": round(size * 1.2, 3),
        "height_px": round(len(widths) * size * 1.2, 3),
        "note": "Advance and line-box estimates only; no glyph shaping, kerning, or ink bounds.",
    }


class GraphicsSession:
    """Owns an in-memory FIFO of generated documents, never arbitrary paths or URLs."""

    tools = staticmethod(tools)

    def __init__(self):
        self._documents = {}

    def close(self):
        self._documents.clear()

    def _render(self, program, format):
        chart = program.get("chart")
        shapes = program.get("shapes", [])
        _expect(bool(shapes or chart), "program", "supply at least one shape or an HTML chart.")
        _expect(chart is None or format == "html", "program.chart", "requires format 'html'.")
        markup = _svg(program) if format == "svg" else _html(program)
        if len(encode(markup)) > MAX_DOCUMENT_WIRE_BYTES:
            fail("GRAPHICS_LIMIT", "Document exceeds the portable result budget; use fewer shapes or shorter text.")
        document_id = "g_" + token_hex(16)
        while document_id in self._documents:
            document_id = "g_" + token_hex(16)
        summary = {
            "document_id": document_id, "format": format, "mime_type": MIME_TYPES[format],
            "title": program.get("title", "Drawing"), "description": program.get("description", ""),
            "bytes": len(markup.encode("utf-8")), "shape_count": len(shapes),
        }
        if format == "svg" or shapes:
            summary["canvas"] = {"width": int(program["width"]), "height": int(program["height"])}
            summary["shape_counts"] = counts = {}
            for shape in shapes:
                kind = shape["type"]
                counts[kind] = counts.get(kind, 0) + 1
        if chart:
            summary["chart"] = {"title": chart.get("title", "Data chart"),
                                "series": [{"label": row["label"], "value": float(_num(row["value"]))}
                                           for row in chart["series"]],
                                "interactions": ["sort_rows_by_value", "restore_input_order"]}
        if len(self._documents) == MAX_DOCUMENTS:
            del self._documents[next(iter(self._documents))]
        self._documents[document_id] = (summary, markup)
        return deepcopy(summary)

    def _get(self, document_id):
        _validate(document_id, _SCHEMAS["display_graphics"]["properties"]["document_id"], "document_id")
        if document_id not in self._documents:
            fail("GRAPHICS_NOT_FOUND", "Document is not available in this connection (unknown or evicted).")
        return self._documents[document_id]

    def embedded_resource(self, document_id):
        """MCP EmbeddedResource content, not a URL to fetch or a UI instruction."""
        summary, markup = self._get(document_id)
        return {"type": "resource", "resource": {"uri": f"on1x-graphics://document/{document_id}",
                "mimeType": summary["mime_type"], "text": markup}}

    def call(self, name, arguments):
        _expect(type(name) is str and name in _SCHEMAS, "tool", "unknown graphics tool.")
        _validate(arguments, _SCHEMAS[name], "arguments", multiline=name == "measure_text")
        if name == "render_graphics":
            return self._render(**arguments)
        if name == "measure_text":
            return _measure(**arguments)
        summary, _ = self._get(arguments["document_id"])
        result = deepcopy(summary)
        result["display"] = "Portable embedded resource; the MCP client decides whether to show it."
        if result["format"] == "html":
            result["sandbox"] = (
                'If previewing, use an isolated iframe with sandbox="allow-scripts" (no allow-same-origin), '
                "preserve the document CSP, and do not inject the markup into a privileged document."
            )
        else:
            result["sandbox"] = "If previewing, treat SVG as an image rather than injecting it as host markup."
        return result
