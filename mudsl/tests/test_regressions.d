module test_regressions;
import MuDSL;

unittest
{
    assert(pattern!"[a-z]+z".matches("az"));
    assert(pattern!"^a+$".matches("aaa"));
    assert(!pattern!"^a+$".matches(""));
    assert(!pattern!"a+$".matches("ab"));
    SourceMap empty;
    assert(empty.line_count() == 1);
    assert(empty.line_column(0) == [1,1]);
    assert(empty.line_text(1) == "");
    auto source = SourceMap("a\nb");
    assert(source.line_column(size_t.max) == [2,2]);
    assert(source.line_column(2) == [2,1]);
}
