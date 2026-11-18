#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/cliche.hpp>
#include <on1x/clone.hpp>
#include <on1x/infer.hpp>
#include <on1x/inherent.hpp>
#include <on1x/nominal.hpp>
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <string_view>

namespace {
using namespace on1x;
using E=Expr::Kind;
using S=Stmt::Kind;

void require(bool condition,const std::string& message){
 if(!condition)throw std::runtime_error(message);
}

template<class F>
void reject(F&& action,std::string_view label,
            std::initializer_list<std::string_view> diagnostics={}){
 try{action();}
 catch(const Error& error){
  if(diagnostics.size()){
   for(auto expected:diagnostics)
    if(error.diagnostic_id==expected)return;
   throw std::runtime_error(std::string(label)+
                            ": wrong diagnostic identity '"+
                            error.diagnostic_id+"' ("+error.what()+")");
  }
  return;
 }
 throw std::runtime_error(std::string(label)+": malformed input was admitted");
}

Unit source(const std::string& text){
 return parse(scan(text,"type-checker-hardening"));
}

Unit checked(const std::string& text){
 auto unit=source(text);check(unit);return unit;
}

void walk_expression(Expr& expression,const std::function<void(Expr&)>& visit);

void walk_body(std::vector<Stmt>& body,
               const std::function<void(Expr&)>& expression_visit,
               const std::function<void(Stmt&)>& statement_visit){
 for(auto& statement:body){
  statement_visit(statement);
  if(statement.value)walk_expression(*statement.value,expression_visit);
  walk_body(statement.body,expression_visit,statement_visit);
  walk_body(statement.otherwise,expression_visit,statement_visit);
 }
}

void walk_expression(Expr& expression,const std::function<void(Expr&)>& visit){
 visit(expression);
 if(expression.callee)walk_expression(*expression.callee,visit);
 if(expression.syntax_tail)walk_expression(*expression.syntax_tail,visit);
 for(auto& child:expression.children)if(child)walk_expression(*child,visit);
 for(auto& proof:expression.proofs)if(proof)walk_expression(*proof,visit);
 walk_body(expression.body,visit,[](Stmt&){});
}

Expr* find_expression(Unit& unit,const std::function<bool(const Expr&)>& predicate){
 Expr* found=nullptr;
 auto expression=[&](Expr& value){if(!found&&predicate(value))found=&value;};
 auto scan=[&](Function& function){
  for(auto& attribute:function.attributes)
   if(attribute)walk_expression(*attribute,expression);
  walk_body(function.body,expression,[](Stmt&){});
 };
 for(auto& function:unit.functions)
  if(function.name=="main")scan(function);
 for(auto& function:unit.functions){
  if(found)break;
  if(function.name!="main"&&!function.prototype)scan(function);
 }
 return found;
}

Stmt* find_statement(Unit& unit,const std::function<bool(const Stmt&)>& predicate){
 Stmt* found=nullptr;
 auto statement=[&](Stmt& value){if(!found&&predicate(value))found=&value;};
 for(auto& function:unit.functions)
  if(function.name=="main")walk_body(function.body,[](Expr&){},statement);
 for(auto& function:unit.functions)
  if(!found&&function.name!="main"&&!function.prototype)
   walk_body(function.body,[](Expr&){},statement);
 return found;
}

Expr& kind(Unit& unit,E expected){
 auto* value=find_expression(unit,[&](const Expr& expression){
  return expression.kind==expected;
 });
 require(value,"missing expression fixture");
 return *value;
}

Expr& call(Unit& unit,std::string_view name){
 auto* value=find_expression(unit,[&](const Expr& expression){
  return expression.kind==E::Call&&expression.callee&&
         expression.callee->text==name;
 });
 require(value,"missing call fixture");
 return *value;
}

void reject_values(Unit& unit,std::string_view label){
 reject([&]{canonical::verify_values(unit);},label,
        {"ON1X-VALUES-VERIFY","ON1X-LITERAL-INTEGER",
         "ON1X-LITERAL-FLOAT","ON1X-LITERAL-STRING",
         "ON1X-LITERAL-CHAR","ON1X-NUMERIC-LITERAL",
         "ON1X-NUMERIC-RANGE","ON1X-NUMERIC-LIMIT"});
}

ExprPtr integer(const Location& where){
 auto value=std::make_unique<Expr>();
 value->kind=E::Integer;value->where=where;value->text="1";
 value->type=Type::I64;return value;
}
}

int main(){
 try{
  {
   auto unit=checked("fn main(){println(1+2);}");
   auto& binary=kind(unit,E::Binary);
   binary.children.front().reset();
   reject_values(unit,"null canonical child");
   reject([&]{(void)emit_c(unit,"null-child");},"null emitter child",
          {"ON1X-CLICHE-HIR","ON1X-IMPL-HIR"});
   reject([&]{(void)mir::lower(unit);},"null MIR child",
          {"ON1X-CLICHE-HIR","ON1X-IMPL-HIR"});
   std::size_t budget=262144;
   reject([&]{(void)clone_unit(unit,budget);},"null cloned child",
          {"ON1X-AST-MALFORMED"});
  }
  {
   auto unit=checked("fn main(){println(1);}");
   auto& value=kind(unit,E::Integer);
   value.proofs.push_back(nullptr);
   reject_values(unit,"null universal proof");
   std::size_t budget=262144;
   reject([&]{(void)clone_unit(unit,budget);},"null cloned proof",
          {"ON1X-AST-MALFORMED"});
  }
  {
   auto unit=checked("fn main(){println(1);}");
   unit.functions.front().attributes.push_back(nullptr);
   std::size_t budget=262144;
   reject([&]{(void)clone_unit(unit,budget);},"null cloned attribute",
          {"ON1X-AST-MALFORMED"});
   reject([&]{(void)emit_c(unit,"null-attribute");},"null emitter attribute",
          {"ON1X-IMPL-HIR"});
  }
  {
   auto unit=checked("fn main(){println(1);}");
   unit.functions.front().body.front().instances.push_back(nullptr);
   reject_values(unit,"stale null specialization");
   std::size_t budget=262144;
   reject([&]{(void)clone_unit(unit,budget);},"null cloned specialization",
          {"ON1X-AST-MALFORMED"});
  }
  {
   Expr packed;
   packed.kind=E::PolyPack;packed.where={"type-checker-hardening",0,1,1};
   packed.proofs.push_back(integer(packed.where));
   packed.proofs.push_back(integer(packed.where));
   reject([&]{source_expression(packed);},"multiple forall source proofs",
          {"ON1X-AST-MALFORMED"});
  }

  {
   auto unit=checked("fn main(){println(1);}");
   kind(unit,E::Integer).type=Type::Hole;
   reject_values(unit,"unresolved checked expression type");
   reject([&]{verify_nominals(unit);},"nominal hole boundary",
          {"ON1X-NOMINAL"});
  }
  {
   auto unit=checked("fn main(){println(1);}");
   kind(unit,E::Integer).type=Type::generic(77);
   reject_values(unit,"free checked inference variable");
   reject([&]{verify_nominals(unit);},"nominal generic boundary",
          {"ON1X-NOMINAL"});
  }
  {
   Unit unit;
   NominalDeclaration declaration;
   declaration.where={"type-checker-hardening",0,1,1};
   declaration.name="Bad";declaration.kind=Type::Record;
   declaration.parameters={"T"};declaration.labels={"value"};
   declaration.fields={Type::bound(1)};
   unit.nominals.push_back(std::move(declaration));
    reject([&]{(void)NominalCatalog(unit);},"unbound nominal declaration type",
           {"ON1X-NOMINAL"});
  }
  {
   auto unit=source("cliche Value { let value: Self; } fn main(){}");
   reject([&]{cliches::verify_elaborated(unit);},
          "unprepared cliche metadata at backend boundary",
          {"ON1X-CLICHE-HIR"});
  }
  {
   Unit unit;
   ClicheDeclaration declaration;
   declaration.where={"type-checker-hardening",0,1,1};
   declaration.name="Malformed";
   ClicheMember member;
   member.where=declaration.where;member.name="value";
   member.kind=ClicheMember::Kind::Value;
   member.result=Type::own(Type::named("Box",{Type::bound(1)}));
   declaration.members.push_back(std::move(member));
   unit.cliches.push_back(std::move(declaration));
   reject([&]{cliches::prepare(unit);},
          "unbound cliche parameter nested under own",
          {"ON1X-CLICHE-SIGNATURE"});
   require(!unit.cliches_prepared&&unit.functions.empty(),
           "failed cliche type validation published state");
  }
  {
   const Location at{"type-checker-hardening",0,1,1};
   auto schema=Type::own(Type::named("Box",{Type::bound(0)}));
   auto instance=cliches::substitute(schema,{Type::I64},at);
   require(instance==Type::own(Type::named("Box",{Type::I64})),
           "cliche substitution skipped an owning target argument");
  }
  {
   auto unit=source(
    "cliche Read { fn read(value:Self)->i64; }"
    "impl Read for i64 { fn read(value:i64)->i64{return value;} }"
    "fn main(){}");
   auto before=unit.functions.size();
   unit.cliche_implementations.front().members.front().
    function.attributes.push_back(nullptr);
   std::size_t budget=262144;
   reject([&]{(void)clone_unit(unit,budget);},
          "null cliche member attribute during clone",
          {"ON1X-AST-MALFORMED"});
   reject([&]{cliches::prepare(unit);},
          "hidden cliche member metadata",
          {"ON1X-CLICHE-HIR"});
   require(!unit.cliches_prepared&&unit.functions.size()==before,
           "malformed cliche preparation published generated functions");
  }
  {
   auto unit=source(
    "record Pair<A,B>{first:A,second:B}"
    "cliche Mark { let marker:i64; }"
    "impl<T> Mark for Pair<T,i64> { let marker:i64=1; }"
    "impl<U> Mark for Pair<i64,U> { let marker:i64=2; }"
    "fn main(){}");
   auto before=unit.functions.size();
   reject([&]{cliches::prepare(unit);},
          "overlapping cliche implementation heads",
          {"ON1X-CLICHE-COHERENCE"});
   require(!unit.cliches_prepared&&unit.functions.size()==before,
           "failed cliche coherence check published generated functions");
   require(std::all_of(
            unit.cliche_implementations.begin(),
            unit.cliche_implementations.end(),
            [](const auto& implementation){
             return std::all_of(
              implementation.members.begin(),implementation.members.end(),
              [](const auto& item){return item.runtime_name.empty();});
            }),
           "failed cliche coherence check published runtime names");
  }
  {
   auto unit=source("fn bad(value:i64)->_ {return value;} fn main(){}");
   unit.functions.front().params.front().type=Type::bound(4);
   reject([&]{infer_types(unit);},"malformed inference annotation",
          {"ON1X-TYPE-FORM"});
   require(unit.functions.front().params.front().type==Type::bound(4),
           "failed annotation validation modified its input");
  }
  {
   auto unit=source("fn main()->_ {println(1+2);}");
   kind(unit,E::Binary).children.front().reset();
   reject([&]{infer_types(unit);},"bounded inference null scan",
          {"ON1X-TYPE-FORM"});
   require(unit.functions.front().result==Type::Hole,
           "failed inference scan modified its input");
  }

  {
   auto unit=checked(
    "fn identity(value:i64)->i64{return value;}"
    "fn main(){println(identity(1));}");
   call(unit,"identity").callee.reset();
   reject_values(unit,"missing checked callee");
  }
  {
   auto unit=checked(
    "fn identity(value:i64)->i64{return value;}"
    "fn main(){println(identity(1));}");
   call(unit,"identity").children.clear();
   reject_values(unit,"checked call arity");
  }
  {
   auto unit=checked(
    "fn identity(value:i64)->i64{return value;}"
    "fn main(){println(identity(1));}");
   auto& invoke=call(unit,"identity");
   invoke.callee->type=Type::function({Type::Bool},Type::I64);
   reject_values(unit,"forged checked callee signature");
  }
  {
   auto unit=checked("fn main(){println(1);}");
   auto& invoke=call(unit,"println");
   invoke.callee->move_out=true;
   reject_values(unit,"forged transfer metadata on intrinsic callee");
   reject([&]{(void)emit_c(unit,"forged-intrinsic-transfer");},
          "emitter forged intrinsic transfer",{"ON1X-VALUES-VERIFY"});
   reject([&]{(void)mir::lower(unit);},"MIR forged intrinsic transfer",
          {"ON1X-VALUES-VERIFY"});
  }
  {
   auto unit=checked("fn main(){println(1);}");
   kind(unit,E::Integer).text="1not_a_literal";
   reject_values(unit,"malformed checked integer");
  }
  {
   auto unit=checked("fn main(){assert(true);}");
   kind(unit,E::Boolean).text="truth";
   reject_values(unit,"malformed checked boolean");
  }
  {
   auto unit=checked("fn main(){println(\"ok\");}");
   kind(unit,E::String).text="\"bad\\q\"";
   reject_values(unit,"malformed checked string");
  }

  {
   auto unit=checked(
    "fn main(){println(match (1,2) with {(left,right)=>left});}");
   auto& project=kind(unit,E::Project);
   project.projection=99;
   reject_values(unit,"tuple projection range");
  }
  {
   auto unit=checked(
    "record Box{value:i64}"
    "fn main(){let box=record<Box>{value:1};println(box[\"value\"]);}");
   auto& field=kind(unit,E::Field);
    field.text="missing";
    reject_values(unit,"record projection label");
   }
   {
    auto unit=checked(
     "fn main(){let captures=capture /ll([a-z])/ ~ \"llk\";"
     "println(captures:1);}");
    auto* capture=find_expression(unit,[](const Expr& expression){
     return expression.kind==E::Index&&expression.text==":";
    });
    require(capture&&capture->children.size()==2&&
            capture->children.front()->type==Type::RegexCaptures,
            "missing canonical regex capture marker fixture");
    canonical::verify_values(unit);
    (void)emit_c(unit,"capture-marker");
    capture->text="forged";
    reject_values(unit,"forged regex capture index marker");
   }
   {
    auto unit=checked(
     "fn main(){let values=array[1,2];println(values[0]);}");
    auto& index=kind(unit,E::Index);
    index.text=":";
    reject_values(unit,"capture marker on an array index");
   }
   {
    auto unit=checked(
     "fn main(){taskgroup{"
     "let ch=->(1,1){payload:i64,capacity:0,mode:uni};"
     "let (tx,rx)=split ch;let worker=launch{let sent=tx <- 42;7};"
     "match <-rx with{Some(value)=>println(value),"
     "None(_)=>println(-1)};println(join worker);}}");
    auto* channel=find_expression(unit,[](const Expr& expression){
     return expression.kind==E::Csp&&expression.text=="channel";
    });
    require(channel&&channel->result==Type::I64,
            "missing validated canonical CSP payload fixture");
    canonical::verify_values(unit);
    (void)emit_c(unit,"csp-payload");
    channel->result=Type::Bool;
    reject_values(unit,"forged canonical CSP payload annotation");
   }
   {
    auto unit=checked(
     "fn main(){"
     "let local=fn(x)->_{return loop{break with x;};};"
     "println(local(8));println(local(false));}");
    auto* generic=find_statement(unit,[](const Stmt& statement){
     return statement.kind==S::Template;
    });
    require(generic&&generic->value&&generic->value->kind==E::Lambda,
            "missing retained local generic template fixture");
    auto* loop=find_expression(unit,[](const Expr& expression){
     return expression.kind==E::Loop;
    });
    require(loop&&loop->control_target,
            "retained generic value loop lost its control identity");
    canonical::verify_values(unit);
    (void)emit_c(unit,"retained-generic-loop");
    loop->control_target=0;
    reject_values(unit,"missing retained generic value-loop identity");
   }
   {
    auto unit=checked(
     "fn main(){"
     "let identity=fn(value)->_{return value;};"
     "println(identity(1));}");
    auto* generic=find_statement(unit,[](const Stmt& statement){
     return statement.kind==S::Template;
    });
    require(generic&&generic->value,
            "missing retained generic transfer-metadata fixture");
    generic->value->move_out=true;
    reject_values(unit,"forged retained generic transfer metadata");
    reject([&]{(void)emit_c(unit,"forged-retained-transfer");},
           "emitter retained generic transfer",{"ON1X-VALUES-VERIFY"});
    reject([&]{(void)mir::lower(unit);},"MIR retained generic transfer",
           {"ON1X-VALUES-VERIFY"});
   }

   {
     auto unit=checked("fn main(){1;}");
   auto& value=kind(unit,E::Integer);
   value.result=Type::Bool;
   reject_values(unit,"forged auxiliary expression result");
  }
  {
   auto unit=checked("fn main(){1;}");
   kind(unit,E::Integer).context=9;
   reject_values(unit,"forged expression syntax context");
  }
  {
   auto unit=checked("fn main(){1;}");
   auto& value=kind(unit,E::Integer);
   value.kind=E::Unit;value.type=Type::Void;value.text="hidden";
   reject_values(unit,"hidden unit text");
  }
  {
   auto unit=checked("fn main(){1;}");
   auto* statement=find_statement(unit,[](const Stmt& value){
    return value.kind==S::Expr;
   });
   require(statement,"missing statement fixture");
   statement->context=7;
   reject_values(unit,"forged statement syntax context");
  }
  {
   auto unit=checked("fn main(){1;}");
   kind(unit,E::Integer).prototype=true;
   reject_values(unit,"forged executable expression prototype");
   reject([&]{verify_nominals(unit);},"nominal prototype boundary",
          {"ON1X-NOMINAL"});
  }
  {
   auto unit=checked("fn main(){1;}");
   unit.functions.front().body.front().prototype=true;
   reject_values(unit,"forged executable statement prototype");
  }
  {
   auto unit=checked("fn main(){1;}");
   TypeParameter parameter;
   parameter.where=unit.functions.front().where;parameter.name="T";
    unit.functions.front().type_parameters.push_back(std::move(parameter));
    reject_values(unit,"source generic metadata on executable function");
  }
  {
   auto unit=checked(
    "fn identity<T>(value:T)->T{return value;}"
    "fn main(){println(identity(1));}");
   auto prototype=std::find_if(
    unit.functions.begin(),unit.functions.end(),
    [](const Function& function){return function.name=="identity";});
   require(prototype!=unit.functions.end()&&prototype->prototype&&
           prototype->type_parameters.size()==1,
           "missing checked generic prototype fixture");
   prototype->type_parameters.front().name.clear();
   reject_values(unit,"forged generic binder metadata");
   reject([&]{(void)emit_c(unit,"forged-generic-binder");},
          "emitter forged generic binder",{"ON1X-VALUES-VERIFY"});
   reject([&]{(void)mir::lower(unit);},"MIR forged generic binder",
          {"ON1X-VALUES-VERIFY"});
  }
  {
   auto unit=checked(
    "cliche Mark { let marker:i64; }"
    "fn identity<T>(value:T)->T{return value;}"
    "fn main(){println(identity(1));}");
   auto prototype=std::find_if(
    unit.functions.begin(),unit.functions.end(),
    [](const Function& function){return function.name=="identity";});
   require(prototype!=unit.functions.end()&&prototype->prototype&&
           prototype->type_parameters.size()==1,
           "missing checked generic constraint fixture");
   ClicheConstraint bound;
   bound.where=prototype->where;bound.name="Mark";
   bound.arguments={Type::bound(0)};
   prototype->type_parameters.front().bounds.push_back(std::move(bound));
   reject_values(unit,"forged detached generic constraint");
   reject([&]{(void)emit_c(unit,"forged-generic-constraint");},
          "emitter forged generic constraint",{"ON1X-VALUES-VERIFY"});
   reject([&]{(void)mir::lower(unit);},"MIR forged generic constraint",
          {"ON1X-VALUES-VERIFY"});
  }
  {
   auto unit=checked("fn main(){1;}");
   auto& value=kind(unit,E::Integer);
   value.kind=E::ClicheMember;value.prototype=true;
   reject([&]{cliches::verify_elaborated(unit);},
          "cliche unresolved executable projection",
          {"ON1X-CLICHE-UNSUPPORTED"});
   reject_values(unit,"forged unresolved prototype projection");
   reject([&]{(void)emit_c(unit,"forged-projection");},
           "emitter unresolved prototype projection",
           {"ON1X-CLICHE-UNSUPPORTED"});
   reject([&]{(void)mir::lower(unit);},
          "MIR unresolved prototype projection",
          {"ON1X-CLICHE-UNSUPPORTED"});
  }
  {
   auto unit=checked("fn main(){1;}");
   auto& value=kind(unit,E::Integer);
   value.kind=E::InherentMember;value.prototype=true;
   reject([&]{inherent::verify_elaborated(unit);},
          "inherent unresolved executable projection",{"ON1X-IMPL"});
   reject_values(unit,"forged unresolved inherent projection");
   reject([&]{(void)emit_c(unit,"forged-inherent-projection");},
          "emitter unresolved inherent projection",{"ON1X-IMPL"});
   reject([&]{(void)mir::lower(unit);},
          "MIR unresolved inherent projection",{"ON1X-IMPL"});
  }
  {
   auto unit=checked("fn main(){1;}");
   kind(unit,E::Integer).syntax_repeat=1;
   reject_values(unit,"source-only expression metadata");
  }

    std::cout
     <<"Type-checker hardening: malformed types, member admission, generic "
       "metadata, ownership, calls, literals and hidden HIR metadata PASS\n";
  return 0;
 }catch(const std::exception& error){
  std::cerr<<error.what()<<'\n';return 1;
 }
}
