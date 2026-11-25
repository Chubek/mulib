#pragma once

#include <on1x/meta.hpp>

// Inspection projection of the checked compiler Unit, not a PPX wire format
// or a loadable IR. The caller must run the normal checked-source pipeline
// before publishing it. No AST round trip or separate admission takes place.
namespace on1x::hir::inspection {
namespace detail {
using meta::SExpr;
using meta::list;
using meta::string;
using meta::symbol;

#define ON1X_HIR_KIND(name) case K::name: return #name
inline const char* kind(Expr::Kind value) {
 using K=Expr::Kind;
 switch(value) {
  ON1X_HIR_KIND(Integer); ON1X_HIR_KIND(Float); ON1X_HIR_KIND(Boolean); ON1X_HIR_KIND(String);
  ON1X_HIR_KIND(Variable); ON1X_HIR_KIND(ClicheMember); ON1X_HIR_KIND(InherentMember);
  ON1X_HIR_KIND(Unary); ON1X_HIR_KIND(Binary); ON1X_HIR_KIND(Call); ON1X_HIR_KIND(Lambda);
  ON1X_HIR_KIND(Tuple); ON1X_HIR_KIND(PolyPack); ON1X_HIR_KIND(PolyProjection);
  ON1X_HIR_KIND(MacroCall); ON1X_HIR_KIND(SyntaxHole); ON1X_HIR_KIND(SyntaxBlock);
  ON1X_HIR_KIND(Extension); ON1X_HIR_KIND(SyntaxList); ON1X_HIR_KIND(SyntaxVector);
  ON1X_HIR_KIND(SyntaxKeyword); ON1X_HIR_KIND(Quote); ON1X_HIR_KIND(Splice); ON1X_HIR_KIND(Lift);
  ON1X_HIR_KIND(Run); ON1X_HIR_KIND(SyntaxLet); ON1X_HIR_KIND(Record); ON1X_HIR_KIND(Variant);
  ON1X_HIR_KIND(Match); ON1X_HIR_KIND(Array); ON1X_HIR_KIND(Field); ON1X_HIR_KIND(Index);
  ON1X_HIR_KIND(Slice); ON1X_HIR_KIND(Length); ON1X_HIR_KIND(PatternMatch); ON1X_HIR_KIND(Case);
  ON1X_HIR_KIND(Unit); ON1X_HIR_KIND(Region); ON1X_HIR_KIND(TagTest); ON1X_HIR_KIND(Payload);
  ON1X_HIR_KIND(Project); ON1X_HIR_KIND(Loop); ON1X_HIR_KIND(Propagate); ON1X_HIR_KIND(SourceCsp);
  ON1X_HIR_KIND(Csp); ON1X_HIR_KIND(UnsafeBlock); ON1X_HIR_KIND(Character); ON1X_HIR_KIND(ByteBuffer);
  ON1X_HIR_KIND(Interpolated); ON1X_HIR_KIND(RegexLiteral); ON1X_HIR_KIND(RegexMatch);
  ON1X_HIR_KIND(RegexCapture); ON1X_HIR_KIND(Borrow);
 }
 throw std::logic_error("unknown On1x HIR expression kind");
}
inline const char* kind(Stmt::Kind value) {
 using K=Stmt::Kind;
 switch(value) {
  ON1X_HIR_KIND(Let); ON1X_HIR_KIND(Assign); ON1X_HIR_KIND(Return); ON1X_HIR_KIND(If);
  ON1X_HIR_KIND(While); ON1X_HIR_KIND(Expr); ON1X_HIR_KIND(Block); ON1X_HIR_KIND(Template);
  ON1X_HIR_KIND(Range); ON1X_HIR_KIND(Break); ON1X_HIR_KIND(Continue); ON1X_HIR_KIND(Storage);
  ON1X_HIR_KIND(Initialize); ON1X_HIR_KIND(Region); ON1X_HIR_KIND(Leave); ON1X_HIR_KIND(Trap);
  ON1X_HIR_KIND(Defer); ON1X_HIR_KIND(PushDefer); ON1X_HIR_KIND(TaskGroup); ON1X_HIR_KIND(CspScope);
 }
 throw std::logic_error("unknown On1x HIR statement kind");
}
inline const char* kind(Pattern::Kind value) {
 using K=Pattern::Kind;
 switch(value) {
  ON1X_HIR_KIND(Name); ON1X_HIR_KIND(Tuple); ON1X_HIR_KIND(Wildcard); ON1X_HIR_KIND(Rest);
  ON1X_HIR_KIND(Literal); ON1X_HIR_KIND(Variant); ON1X_HIR_KIND(Array); ON1X_HIR_KIND(Record);
  ON1X_HIR_KIND(Or); ON1X_HIR_KIND(As);
 }
 throw std::logic_error("unknown On1x HIR pattern kind");
}
inline const char* kind(Expr::Reference value) {
 using K=Expr::Reference;
 switch(value) {
  ON1X_HIR_KIND(Unresolved); ON1X_HIR_KIND(Local); ON1X_HIR_KIND(Function); ON1X_HIR_KIND(Extern);
  ON1X_HIR_KIND(Intrinsic); ON1X_HIR_KIND(UnsafeForeign); ON1X_HIR_KIND(ScalarForeign);
  ON1X_HIR_KIND(BorrowFunction);
 }
 throw std::logic_error("unknown On1x HIR reference kind");
}
inline const char* kind(BorrowMode value) {
 using K=BorrowMode;
 switch(value) { ON1X_HIR_KIND(Value); ON1X_HIR_KIND(Shared); ON1X_HIR_KIND(Exclusive); }
 throw std::logic_error("unknown On1x HIR borrow mode");
}
#undef ON1X_HIR_KIND

inline SExpr number(std::size_t value) { return symbol(std::to_string(value)); }
inline SExpr at(const Location& where) {
 return list({symbol("at"),string(where.file),number(where.offset),number(where.line),number(where.column)});
}
inline SExpr tagged(const char* tag,SExpr value) {
 std::vector<SExpr> items;
 items.push_back(symbol(tag)); items.push_back(std::move(value));
 return list(std::move(items));
}
class Writer {
 std::size_t nodes=0, bytes=0;
 void charge(const Location& where,std::size_t depth,std::size_t strings=0) {
  if(depth>64 || ++nodes>12000 || strings>meta::max_wire_bytes-bytes)
   throw Error(where,"checked HIR inspection exceeds its output budget","ON1X-HIR-LIMIT");
  bytes+=strings;
 }
 SExpr type(const Type& value,const Location& where,std::size_t depth) {
  auto text=type_name(value);
  charge(where,depth,text.size());
  return string(std::move(text));
 }
 SExpr labels(const std::vector<std::string>& names,const Location& where,std::size_t depth) {
  std::vector<SExpr> fields;
  for(const auto& name:names) { charge(where,depth,name.size()); fields.push_back(string(name)); }
  return list(std::move(fields));
 }
 SExpr pat(const Pattern& p,std::size_t depth) {
  charge(p.where,depth,p.name.size()+p.where.file.size());
  std::vector<SExpr> fields;
  for(const auto& child:p.elements)fields.push_back(pat(child,depth+1));
  std::vector<SExpr> out{symbol(kind(p.kind)),string(p.name),type(p.type,p.where,depth),at(p.where)};
  if(p.open)out.push_back(symbol("open"));
  if(!p.labels.empty())out.push_back(tagged("labels",labels(p.labels,p.where,depth)));
  if(!fields.empty())out.push_back(tagged("elements",list(std::move(fields))));
  return list(std::move(out));
 }
 SExpr params(const std::vector<Param>& values,std::size_t depth) {
  std::vector<SExpr> result;
  for(const auto& p:values) {
   charge(p.where,depth,p.name.size()+p.where.file.size());
   std::vector<SExpr> parameter{symbol("param"),string(p.name),type(p.type,p.where,depth),
                                symbol(kind(p.borrow)),at(p.where)};
   if(p.pattern)parameter.push_back(pat(*p.pattern,depth+1));
   result.push_back(list(std::move(parameter)));
  }
  return tagged("params",list(std::move(result)));
 }
 SExpr expr(const Expr& e,std::size_t depth) {
  charge(e.where,depth,e.text.size()+e.where.file.size());
  std::vector<SExpr> out{symbol(kind(e.kind)),type(e.type,e.where,depth),at(e.where)};
  if(!e.text.empty())out.push_back(tagged("text",string(e.text)));
  if(e.reference!=Expr::Reference::Unresolved)
   out.push_back(tagged("reference",symbol(kind(e.reference))));
  if(e.kind==Expr::Kind::Project || e.kind==Expr::Kind::Payload || e.kind==Expr::Kind::TagTest ||
     e.kind==Expr::Kind::PolyProjection)
   out.push_back(tagged("projection",number(e.projection)));
  if(e.borrow!=BorrowMode::Value)out.push_back(tagged("borrow",symbol(kind(e.borrow))));
  if(e.unsafe_region)out.push_back(symbol("unsafe-region"));
  if(e.move_out)out.push_back(symbol("move-out"));
  if(e.noreturn)out.push_back(symbol("noreturn"));
  if(!e.labels.empty())out.push_back(tagged("labels",labels(e.labels,e.where,depth)));
  if(!e.patterns.empty()) {
   std::vector<SExpr> patterns;
   for(const auto& p:e.patterns)patterns.push_back(pat(p,depth+1));
   out.push_back(tagged("patterns",list(std::move(patterns))));
  }
  if(e.callee)out.push_back(tagged("callee",expr(*e.callee,depth+1)));
  if(!e.children.empty()) {
   std::vector<SExpr> operands;
   for(const auto& child:e.children)operands.push_back(child ? expr(*child,depth+1) : symbol("none"));
   out.push_back(tagged("operands",list(std::move(operands))));
  }
  if(e.kind==Expr::Kind::Lambda) {
   out.push_back(params(e.parameters,depth+1));
   out.push_back(tagged("result",type(e.result,e.where,depth)));
   std::vector<SExpr> captures;
   for(const auto& capture:e.captures) {
    charge(e.where,depth,capture.name.size());
    captures.push_back(list({string(capture.name),type(capture.type,e.where,depth)}));
   }
   if(!captures.empty())out.push_back(tagged("captures",list(std::move(captures))));
  }
  if(!e.body.empty())out.push_back(tagged("body",body(e.body,depth+1)));
  return list(std::move(out));
 }
 SExpr stmt(const Stmt& s,std::size_t depth) {
  charge(s.where,depth,s.name.size()+s.label.size()+s.where.file.size());
  std::vector<SExpr> out{symbol(kind(s.kind)),at(s.where)};
  if(!s.name.empty())out.push_back(tagged("name",string(s.name)));
  if(!s.label.empty())out.push_back(tagged("label",string(s.label)));
  if(s.control_target)out.push_back(tagged("control-target",number(s.control_target)));
  if(s.declared!=Type::Invalid)out.push_back(tagged("type",type(s.declared,s.where,depth)));
  if(s.ref_store)out.push_back(symbol("ref-store"));
  if(s.mutable_binding)out.push_back(symbol("mutable"));
  if(s.unsafe_block)out.push_back(symbol("unsafe-block"));
  if(s.prototype)out.push_back(symbol("prototype"));
  if(s.pattern)out.push_back(pat(*s.pattern,depth+1));
  if(s.value)out.push_back(tagged("value",expr(*s.value,depth+1)));
  if(!s.body.empty())out.push_back(tagged("body",body(s.body,depth+1)));
  if(!s.otherwise.empty())out.push_back(tagged("otherwise",body(s.otherwise,depth+1)));
  return list(std::move(out));
 }
 SExpr body(const std::vector<Stmt>& statements,std::size_t depth) {
  std::vector<SExpr> items;
  for(const auto& s:statements)items.push_back(stmt(s,depth));
  return list(std::move(items));
 }
public:
 SExpr unit(const Unit& u) {
  std::vector<SExpr> functions;
  for(const auto& f:u.functions) {
   charge(f.where,0,f.name.size()+f.where.file.size());
   std::vector<SExpr> item{symbol(f.external?"extern":f.procedure?"procedure":"function"),
                           string(f.name),at(f.where)};
   item.push_back(params(f.params,1));
   item.push_back(tagged("result",type(f.result,f.where,0)));
   if(f.prototype)item.push_back(symbol("prototype"));
   if(!f.kernel_symbol.empty()) {
    charge(f.where,0,f.kernel_symbol.size());
    item.push_back(tagged("foreign-symbol",string(f.kernel_symbol)));
   }
   if(!f.external)item.push_back(tagged("body",body(f.body,1)));
   functions.push_back(list(std::move(item)));
  }
  std::vector<SExpr> nominals;
  for(const auto& declaration:u.nominals) {
   const auto& where=declaration.where;
   charge(where,0,declaration.name.size()+where.file.size());
   std::vector<SExpr> fields;
   for(const auto& field:declaration.fields)fields.push_back(type(field,where,1));
   nominals.push_back(list({
    symbol(declaration.kind==Type::Record?"record":"enum"),string(declaration.name),at(where),
    tagged("params",labels(declaration.parameters,where,1)),
    tagged("labels",labels(declaration.labels,where,1)),
    tagged("fields",list(std::move(fields))),
   }));
  }
  std::vector<SExpr> root{symbol("on1x.hir"),symbol("1")};
  root.push_back(tagged("nominals",list(std::move(nominals))));
  root.push_back(tagged("functions",list(std::move(functions))));
  return list(std::move(root));
 }
};
} // namespace detail

inline std::string write_checked(const Unit& unit) {
 return meta::write_sexpr(detail::Writer{}.unit(unit));
}
} // namespace on1x::hir::inspection
