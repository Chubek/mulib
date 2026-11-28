"""Pygments lexer used by minted and available to editor/documentation tools."""
from pygments.lexer import RegexLexer, words
from pygments.token import Comment, Keyword, Name, Number, Operator, Punctuation, String, Whitespace


class MuLLLexer(RegexLexer):
    name = "MuLL"
    aliases = ["mull"]
    filenames = ["*.mull"]
    mimetypes = ["text/x-mull"]
    tokens = {
        "root": [
            (r"\s+", Whitespace),
            (r"(?:#|//)[^\n]*", Comment.Single),
            (r'"', String.Double, "string"),
            (words(("val", "var", "ref", "mulib", "fn", "if", "else", "while", "return", "break"), suffix=r"\b"), Keyword),
            (words(("true", "false", "nil"), suffix=r"\b"), Keyword.Constant),
            (words(("and", "or", "not"), suffix=r"\b"), Operator.Word),
            (r"[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?", Number),
            (r"\bG\b", Name.Builtin),
            (r"[A-Za-z_][A-Za-z_0-9]*", Name),
            (r"<-|==|!=|<=|>=|[+*/%<>!\-]", Operator),
            (r"[(){}\[\]:;,]", Punctuation),
        ],
        "string": [
            (r'\\[nrt0\\"]', String.Escape),
            (r"\$E\?", String.Interpol),
            (r'"', String.Double, "#pop"),
            (r'[^"\\$]+', String.Double),
            (r"\$", String.Double),
        ],
    }
