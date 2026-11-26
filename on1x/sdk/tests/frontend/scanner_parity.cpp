#include "On1xSDK-Lexer.h"
#include "On1xSDK-Options.h"
#include "On1xSDK-Parser.h"
#include "On1xSDK-PPX.h"
#include "On1xSDK-Session.h"
#include "On1xSDK-Source.h"

#include <on1x/ast.hpp>

#include <cassert>
#include <string>
#include <vector>

int main() {
    const std::vector<std::string> samples = {
        "",
        "# comment\r\nfn main(){}",
        "let_name+12;\n",
        "fn main(){let s=\"snow: \xe9\x9b\xaa\";println(s);}",
        "fn main(){let c=quote{lift(40)+2};println(run(c));}",
        "fn main(){let r=/[a-z]+/;assert(match r ~ \"abc\");}",
        "proc main(){let action=proc(){return;};action();}",
        "operator(lexeme:\"??\",lexeme2:\"?|\",arity:\"ternary\",formals:a,b,c){return b;}"
        "infix 4 ??;infixr 2 ??;fn main(){let op=operator(\"??\");let x=1??2?|3;}",
        "fn main(){let a=r!\"raw\";let b=b!\"bytes\";"
        "let c=#\\a;let x=1.5f32;}",
        "{! main doc }{- skipped -}fn main(){let a='''a\n b''';"
        "let b=r!'''raw''';let c=b!'''bytes''';}{> trailing doc }",
        "macro m{rule($x:Expression)=>expression{$x+1};}"
        "fn main(){println(m!(41));}"
    };

    On1xSDK_Session *session = nullptr;
    On1xSDK_Options *options = nullptr;
    assert(on1x_sdk_session_create(nullptr, &session) == ON1X_SDK_OK);
    assert(on1x_sdk_options_create(session, &options) == ON1X_SDK_OK);

    for (std::size_t sample = 0; sample < samples.size(); ++sample) {
        const auto path = "virtual/parity-" + std::to_string(sample) + ".on1x";
        const auto direct = on1x::scan(samples[sample], path);
        On1xSDK_Source *source = nullptr;
        On1xSDK_SourceSnapshot *revision = nullptr;
        On1xSDK_TokenStream *tokens = nullptr;
        assert(on1x_sdk_source_create(
            session, path.data(), path.size(), samples[sample].data(),
            samples[sample].size(), ON1X_SDK_SOURCE_BYTES,
            &source) == ON1X_SDK_OK);
        assert(on1x_sdk_source_snapshot(source, &revision) == ON1X_SDK_OK);
        assert(on1x_sdk_lex(revision, options, &tokens) == ON1X_SDK_OK);
        std::size_t count = 0;
        assert(on1x_sdk_token_stream_count(tokens, &count) == ON1X_SDK_OK);
        assert(count == direct.size());
        for (std::size_t i = 0; i < count; ++i) {
            On1xSDK_TokenInfo token =
                ON1X_SDK_COMPILER_INIT(On1xSDK_TokenInfo);
            assert(on1x_sdk_token_stream_get(tokens, i, &token) ==
                   ON1X_SDK_OK);
            assert(std::string_view(token.text, token.text_size) ==
                   direct[i].text);
            assert(token.byte_begin == direct[i].where.offset);
            assert(token.line == direct[i].where.line);
            assert(token.column == direct[i].where.column);
            const auto spelling =
                std::string_view(token.text, token.text_size);
             if (spelling.starts_with("r!\"") ||
                 spelling.starts_with("b!\"") ||
                 spelling.starts_with("'''") ||
                 spelling.starts_with("r!'''") ||
                 spelling.starts_with("b!'''"))
                assert(token.kind == ON1X_SDK_TOKEN_STRING);
            if (spelling.starts_with("#\\"))
                assert(token.kind == ON1X_SDK_TOKEN_CHARACTER);
            if (spelling == "1.5f32")
                assert(token.kind == ON1X_SDK_TOKEN_FLOAT);
             if (spelling == "proc")
                 assert(token.kind == ON1X_SDK_TOKEN_KEYWORD);
            if (spelling == "operator" || spelling == "infix" || spelling == "infixr")
                assert(token.kind == ON1X_SDK_TOKEN_KEYWORD);
            if (spelling == "??" || spelling == "?|")
                assert(token.kind == ON1X_SDK_TOKEN_CUSTOM_OPERATOR);
            if (spelling == "\"??\"" || spelling == "\"?|\"")
                assert(token.kind == ON1X_SDK_TOKEN_STRING);
            if (spelling == "+" || spelling == ";" || spelling == "=")
                assert(token.kind == ON1X_SDK_TOKEN_PUNCTUATION);
        }
        if (samples[sample].starts_with("{! main doc }")) {
            On1xSDK_AST *ast = nullptr;
            assert(on1x_sdk_parse(tokens, options, &ast) == ON1X_SDK_OK);
            std::size_t size = 0;
            assert(on1x_sdk_ppx_encode(ast, nullptr, 0, &size) == ON1X_SDK_OK);
            std::string wire(size, '\0');
            assert(on1x_sdk_ppx_encode(ast, wire.data(), size, &size) == ON1X_SDK_OK);
            assert(wire.find("(Docstring \"!\" \" main doc \"") != std::string::npos);
            assert(wire.find("(Docstring \">\" \" trailing doc \"") != std::string::npos);
            on1x_sdk_ast_release(ast);
        }
        on1x_sdk_token_stream_release(tokens);
        on1x_sdk_source_revision_release(revision);
        on1x_sdk_source_release(source);
    }

    on1x_sdk_options_release(options);
    on1x_sdk_session_release(session);
}
