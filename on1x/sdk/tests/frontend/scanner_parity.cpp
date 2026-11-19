#include "On1xSDK-Lexer.h"
#include "On1xSDK-Options.h"
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
        "fn main(){let a=r!\"raw\";let b=b!\"bytes\";"
        "let c=#\\a;let x=1.5f32;}",
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
        On1xSDK_SourceRevision *revision = nullptr;
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
                spelling.starts_with("b!\""))
                assert(token.kind == ON1X_SDK_TOKEN_STRING);
            if (spelling.starts_with("#\\"))
                assert(token.kind == ON1X_SDK_TOKEN_CHARACTER);
            if (spelling == "1.5f32")
                assert(token.kind == ON1X_SDK_TOKEN_FLOAT);
        }
        on1x_sdk_token_stream_release(tokens);
        on1x_sdk_source_revision_release(revision);
        on1x_sdk_source_release(source);
    }

    on1x_sdk_options_release(options);
    on1x_sdk_session_release(session);
}
