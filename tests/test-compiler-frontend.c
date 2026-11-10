#define MULIB_IMPL_LEXICAL_TOKEN
#define MULIB_IMPL_LEXER
#define MULIB_IMPL_LEXGEN
#define MULIB_IMPL_PARSER_PRATT
#define MULIB_IMPL_SYMBOLTABLE
#define MULIB_IMPL_COMPILER_SCOPE
#define MULIB_IMPL_CONSTEVAL
#define MULIB_IMPL_NAME_RESOLUTION
#define MULIB_IMPL_TYPESYSTEM
#define MULIB_IMPL_TYPECHECK
#define MULIB_IMPL_CONSTRAINTS
#define MULIB_IMPL_SYNTAX_TREE
#include "mulib-lexgen.h"
#include "mulib-compiler-scope.h"
#include "mulib-consteval.h"
#include "mulib-name-resolution.h"
#include "mulib-typecheck.h"
#include "mulib-constraints.h"
#include "mulib-syntax-tree.h"
#include <assert.h>

int main(void) {
    mulib_lexer_t lex;mulib_lexical_token_t tok;
    const char input[]="// comment\n x12 == 42 /*ok*/ \"a\\\"b\"";
    mulib_lexer_init(&lex,input,sizeof input-1);
    assert(mulib_lexer_next(&lex,&tok)==1&&tok.kind==MULIB_TOKEN_IDENTIFIER&&tok.offset==12);
    assert(mulib_lexical_token_is(&tok,"x12")&&!mulib_lexical_token_is(&tok,"x"));
    assert(mulib_lexer_next(&lex,&tok)==1&&mulib_lexical_token_is(&tok,"=="));
    assert(mulib_lexer_next(&lex,&tok)==1&&tok.kind==MULIB_TOKEN_INTEGER);
    assert(mulib_lexer_next(&lex,&tok)==1&&tok.kind==MULIB_TOKEN_STRING);
    assert(!mulib_lexer_next(&lex,&tok)&&tok.kind==MULIB_TOKEN_EOF);
    assert(!mulib_lexer_next(&lex,&tok));
    mulib_lexer_init(&lex,"/*",2);
    assert(mulib_lexer_next(&lex,&tok)==1&&tok.kind==MULIB_TOKEN_ERROR&&tok.length==2);
    mulib_lexer_init(&lex,"'",1);
    assert(mulib_lexer_next(&lex,&tok)==1&&tok.kind==MULIB_TOKEN_ERROR);
    mulib_lexgen_rule_t rules[]={{MULIB_LEXGEN_LITERAL,"if",1},{MULIB_LEXGEN_IDENT,NULL,2},
        {MULIB_LEXGEN_DIGITS,NULL,3},{MULIB_LEXGEN_WHITESPACE,NULL,4}};
    size_t matched=99;int token=99;
    assert(mulib_lexgen_match(rules,4,"if",2,&matched,&token)==1&&token==1&&matched==2);
    assert(mulib_lexgen_match(rules,4,"iffy",4,&matched,&token)==1&&token==2&&matched==4);
    assert(!mulib_lexgen_match(rules,4,"?",1,&matched,&token)&&matched==4);
    rules[0].kind=(mulib_lexgen_rule_kind_t)-1;
    assert(mulib_lexgen_match(rules,4,"if",2,&matched,&token)==-1);

    mulib_compiler_scope_t scope={0};uint64_t value=99;
    assert(mulib_compiler_scope_bind(&scope,"x",6));
    assert(!mulib_compiler_scope_bind(&scope,"x",7));
    assert(mulib_symboltable_enter(&scope));
    assert(mulib_compiler_scope_bind(&scope,"x",7));
    assert(mulib_compiler_scope_resolve(&scope,"x",&value)&&value==7);
    assert(mulib_symboltable_leave(&scope)&&!mulib_symboltable_leave(&scope));
    assert(mulib_compiler_scope_resolve(&scope,"x",&value)&&value==6);
    mulib_parser_pratt_t tree={0};size_t error;
    const char *expr="+x * (8 - 1)";
    assert(mulib_parser_pratt_parse(expr,strlen(expr),&tree,&error)&&error==strlen(expr));
    int64_t result=99;
    assert(mulib_consteval_eval(&tree,&scope,&result)&&result==42);
    size_t ids[16],failure=0;
    assert(mulib_name_resolution_run(&tree,&scope,ids,16,&failure)&&failure==SIZE_MAX&&ids[0]==6);
    ids[0]=123;
    assert(!mulib_name_resolution_run(&tree,NULL,ids,16,&failure)&&failure==0&&ids[0]==123);
    mulib_parser_pratt_free(&tree);
    const char *invalid[]={"","1 +","1 2","(1","1)","9223372036854775808","\"s\"","/*"};
    for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++) {
        assert(!mulib_parser_pratt_parse(invalid[i],strlen(invalid[i]),&tree,&error));
        assert(!tree.nodes&&!tree.count&&error<=strlen(invalid[i]));
    }
    char deep[300];memset(deep,'-',sizeof deep);deep[299]='1';
    assert(!mulib_parser_pratt_parse(deep,sizeof deep,&tree,&error));
    assert(mulib_parser_pratt_parse("9223372036854775807+1",21,&tree,&error));
    result=77;assert(!mulib_consteval_eval(&tree,NULL,&result)&&result==77);
    mulib_parser_pratt_free(&tree);
    assert(mulib_parser_pratt_parse("20-5-3",6,&tree,NULL));
    assert(mulib_consteval_eval(&tree,NULL,&result)&&result==12);mulib_parser_pratt_free(&tree);

    mulib_typesystem_t types={0};size_t integer,boolean,again,fn;
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_I64,9,9,9},&integer));
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_I64,0,0,0},&again)&&again==integer);
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_BOOL,0,0,0},&boolean));
    assert(!mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_POINTER,99,0,0},&again));
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_FUNCTION,integer,0,99},&fn));
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_FUNCTION,integer,0,0},&again)&&again==fn);
    assert(mulib_typesystem_get(&types,integer)->kind==MULIB_TYPE_I64&&!mulib_typesystem_get(&types,99));
    mulib_symboltable_free(&scope);assert(mulib_symboltable_define(&scope,"x",integer));
    assert(mulib_parser_pratt_parse("x < 7",5,&tree,NULL));
    assert(mulib_typecheck_expr(&tree,&types,&scope,&again)&&again==boolean);
    assert(!mulib_typecheck_expr(&tree,&types,NULL,&again));mulib_parser_pratt_free(&tree);
    assert(mulib_parser_pratt_parse("(1<2)+3",7,&tree,NULL));
    assert(!mulib_typecheck_expr(&tree,&types,&scope,&again));mulib_parser_pratt_free(&tree);
    mulib_typesystem_free(&types);mulib_symboltable_free(&scope);
    mulib_constraints_t c={0};size_t a,b,d;
    assert(mulib_constraints_new(&c,&a)&&mulib_constraints_new(&c,&b)&&mulib_constraints_new(&c,&d));
    assert(!mulib_constraints_type(&c,a,&again));
    assert(mulib_constraints_bind(&c,a,3)&&mulib_constraints_unify(&c,a,b));
    assert(mulib_constraints_type(&c,b,&again)&&again==3);
    assert(mulib_constraints_bind(&c,d,4)&&!mulib_constraints_unify(&c,b,d));
    assert(!mulib_constraints_bind(&c,b,4)&&!mulib_constraints_bind(&c,d,SIZE_MAX));
    assert(mulib_constraints_type(&c,d,&again)&&again==4);mulib_constraints_free(&c);
    mulib_syntax_tree_t syntax={0};size_t root,child,sibling;
    assert(mulib_syntax_tree_add(&syntax,SIZE_MAX,0,0,10,&root));
    assert(mulib_syntax_tree_add(&syntax,root,1,1,3,&child));
    assert(mulib_syntax_tree_add(&syntax,root,1,5,3,&sibling));
    assert(mulib_syntax_tree_validate(&syntax));
    assert(!mulib_syntax_tree_add(&syntax,root,1,9,2,&again)&&syntax.count==3);
    syntax.nodes[child].next_sibling=child;assert(!mulib_syntax_tree_validate(&syntax));
    syntax.nodes[child].next_sibling=sibling;syntax.nodes[root].last_child=child;
    assert(!mulib_syntax_tree_validate(&syntax));syntax.nodes[root].last_child=sibling;
    syntax.nodes[root].first_child=sibling;assert(!mulib_syntax_tree_validate(&syntax));
    mulib_syntax_tree_free(&syntax);
    return 0;
}
